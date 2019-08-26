#include <cstdint>
#include <cstring>
#include <iostream>

#include "conn.hh"
#include "debug.hh"
#include "reader.hh"
#include "rtp_hevc.hh"
#include "queue.hh"
#include "send.hh"
#include "writer.hh"

#define RTP_FRAME_MAX_DELAY           50
#define INVALID_SEQ           0x13371338

enum FRAG_TYPES {
    FT_INVALID   = -2, /* invalid combination of S and E bits */
    FT_NOT_FRAG  = -1, /* frame doesn't contain HEVC fragment */
    FT_START     =  1, /* frame contains a fragment with S bit set */
    FT_MIDDLE    =  2, /* frame is fragment but not S or E fragment */
    FT_END       =  3, /* frame contains a fragment with E bit set */
};

struct hevc_fu_info {
    kvz_rtp::clock::hrc::hrc_t sframe_time; /* clock reading when the first fragment is received */
    uint32_t sframe_seq;                    /* sequence number of the frame with s-bit */
    uint32_t eframe_seq;                    /* sequence number of the frame with e-bit */
    size_t pkts_received;                   /* how many fragments have been received */
    size_t total_size;                      /* total size of all fragments */
};

static int __get_next_frame_start(uint8_t *data, uint32_t offset, uint32_t data_len, uint8_t& start_len)
{
    uint8_t zeros = 0;
    uint32_t pos  = 0;

    while (offset + pos < data_len) {
        if (zeros >= 2 && data[offset + pos] == 1) {
            start_len = zeros + 1;
            return offset + pos + 1;
        }

        if (data[offset + pos] == 0)
            zeros++;
        else
            zeros = 0;

        pos++;
    }

    return -1;
}

static rtp_error_t __push_hevc_frame(kvz_rtp::connection *conn, uint8_t *data, size_t data_len, bool more)
{
    uint8_t nalType  = (data[0] >> 1) & 0x3F;
    rtp_error_t ret  = RTP_OK;
    size_t data_left = data_len;
    size_t data_pos  = 0;

    if (data_len <= MAX_PAYLOAD) {
        LOG_DEBUG("send unfrag size %zu, type %u", data_len, nalType);
        return kvz_rtp::generic::push_frame(conn, data, data_len, 0);
    }

    LOG_DEBUG("send frag size: %zu, type %u", data_len, nalType);

#ifdef __linux__
    kvz_rtp::frame_queue *fqueue = conn->get_frame_queue();
    fqueue->init_queue(conn);

    /* all fragment units share the same RTP and HEVC NAL headers
     * but because there's three different types of FU headers (and because the state 
     * of each buffer must last between calls) we must allocate space for three FU headers */
    std::vector<std::pair<size_t, uint8_t *>> buffers;

    uint8_t nal_header[kvz_rtp::frame::HEADER_SIZE_HEVC_NAL] = {
        49 << 1, /* fragmentation unit */
        1,       /* TID */
    };

    /* one for first frag, one for all the middle frags and one for the last frag */
    uint8_t fu_headers[3 * kvz_rtp::frame::HEADER_SIZE_HEVC_FU] = {
        (uint8_t)((1 << 7) | nalType),
        nalType,
        (uint8_t)((1 << 6) | nalType)
    };

    buffers.push_back(std::make_pair(sizeof(nal_header), nal_header));
    buffers.push_back(std::make_pair(sizeof(uint8_t),    &fu_headers[0]));
    buffers.push_back(std::make_pair(MAX_PAYLOAD,        nullptr));

    data_pos   = kvz_rtp::frame::HEADER_SIZE_HEVC_NAL;
    data_left -= kvz_rtp::frame::HEADER_SIZE_HEVC_NAL;

    while (data_left > MAX_PAYLOAD) {
        buffers.at(2).first  = MAX_PAYLOAD;
        buffers.at(2).second = &data[data_pos];

        if ((ret = fqueue->enqueue_message(conn, buffers)) != RTP_OK)
            return ret;

        data_pos  += MAX_PAYLOAD;
        data_left -= MAX_PAYLOAD;

        /* from now on, use the FU header meant for middle fragments */
        buffers.at(1).second = &fu_headers[1];
    }

    /* use the FU header meant for the last fragment */
    buffers.at(1).second = &fu_headers[2];

    buffers.at(2).first  = data_left;
    buffers.at(2).second = &data[data_pos];

    if ((ret = fqueue->enqueue_message(conn, buffers)) != RTP_OK) {
        LOG_ERROR("Failed to send HEVC frame!");
        fqueue->empty_queue();
        return ret;
    }

    if (more)
        return RTP_NOT_READY;

    return fqueue->flush_queue(conn);
#else
    const size_t HEADER_SIZE =
        kvz_rtp::frame::HEADER_SIZE_RTP +
        kvz_rtp::frame::HEADER_SIZE_HEVC_NAL +
        kvz_rtp::frame::HEADER_SIZE_HEVC_FU;

    uint8_t buffer[HEADER_SIZE + MAX_PAYLOAD];

    conn->fill_rtp_header(buffer);

    buffer[kvz_rtp::frame::HEADER_SIZE_RTP + 0]  = 49 << 1;            /* fragmentation unit */
    buffer[kvz_rtp::frame::HEADER_SIZE_RTP + 1]  = 1;                  /* TID */
    buffer[kvz_rtp::frame::HEADER_SIZE_RTP +
           kvz_rtp::frame::HEADER_SIZE_HEVC_NAL] = (1 << 7) | nalType; /* Start bit + NAL type */

    data_pos   = kvz_rtp::frame::HEADER_SIZE_HEVC_NAL;
    data_left -= kvz_rtp::frame::HEADER_SIZE_HEVC_NAL;

    while (data_left > MAX_PAYLOAD) {
        memcpy(&buffer[HEADER_SIZE], &data[data_pos], MAX_PAYLOAD);

        if ((ret = kvz_rtp::send::send_frame(conn, buffer, sizeof(buffer))) != RTP_OK)
            return RTP_GENERIC_ERROR;

        conn->update_rtp_sequence(buffer);

        data_pos  += MAX_PAYLOAD;
        data_left -= MAX_PAYLOAD;

        /* Clear extra bits */
        buffer[kvz_rtp::frame::HEADER_SIZE_RTP +
               kvz_rtp::frame::HEADER_SIZE_HEVC_NAL] = nalType;
    }

    buffer[kvz_rtp::frame::HEADER_SIZE_RTP +
           kvz_rtp::frame::HEADER_SIZE_HEVC_NAL] |= (1 << 6); /* set E bit to signal end of data */

    memcpy(&buffer[HEADER_SIZE], &data[data_pos], data_left);

    ret = kvz_rtp::send::send_frame(conn, buffer, HEADER_SIZE + data_left);
#endif

    return ret;
}

rtp_error_t kvz_rtp::hevc::push_frame(kvz_rtp::connection *conn, uint8_t *data, size_t data_len, int flags)
{
    uint8_t start_len;
    int32_t prev_offset = 0;
    int offset = __get_next_frame_start(data, 0, data_len, start_len);
    prev_offset = offset;

    while (offset != -1) {
        offset = __get_next_frame_start(data, offset, data_len, start_len);

        if (offset > 4 && offset != -1) {
            if (__push_hevc_frame(conn, &data[prev_offset], offset - prev_offset - start_len, true) == -1)
                return RTP_GENERIC_ERROR;

            prev_offset = offset;
        }
    }

    if (prev_offset == -1)
        prev_offset = 0;

    return __push_hevc_frame(conn, &data[prev_offset], data_len - prev_offset, false);
}

static int __check_frame(kvz_rtp::frame::rtp_frame *frame)
{
    bool first_frag = frame->payload[2] & 0x80;
    bool last_frag  = frame->payload[2] & 0x40;

    if ((frame->payload[0] >> 1) != 49)
        return FT_NOT_FRAG;

    if (first_frag && last_frag)
        return FT_INVALID;

    if (first_frag)
        return FT_START;

    if (last_frag)
        return FT_END;

    return FT_MIDDLE;
}

rtp_error_t kvz_rtp::hevc::frame_receiver(kvz_rtp::reader *reader)
{
    LOG_INFO("frameReceiver starting listening...");

    int nread = 0;
    rtp_error_t ret;
    sockaddr_in sender_addr;
    kvz_rtp::socket socket = reader->get_socket();
    kvz_rtp::frame::rtp_frame *frame, *frames[0xffff + 1] = { 0 };

    uint8_t nal_header[2] = { 0 };
    std::map<uint32_t, hevc_fu_info> s_timers;
    std::map<uint32_t, size_t> dropped_frames;

    while (reader->active()) {
        ret = socket.recvfrom(reader->get_recv_buffer(), reader->get_recv_buffer_len(), 0, &sender_addr, &nread);

        if (ret != RTP_OK) {
            LOG_ERROR("recvfrom failed! FrameReceiver cannot continue!");
            return RTP_GENERIC_ERROR;;
        }

        if ((frame = reader->validate_rtp_frame(reader->get_recv_buffer(), nread)) == nullptr) {
            LOG_DEBUG("received an invalid frame, discarding");
            continue;
        }
        memcpy(&frame->src_addr, &sender_addr, sizeof(sockaddr_in));

        /* Update session related statistics
         * If this is a new peer, RTCP will take care of initializing necessary stuff
         *
         * Skip processing the packet if it was invalid. This is mostly likely caused
         * by an SSRC collision */
        if (reader->update_receiver_stats(frame) != RTP_OK)
            continue;

        /* How to the frame is handled is based what its type is. Generic and Opus frames
         * don't require any extra processing so they can be returned to the user as soon as
         * they're received without any buffering.
         *
         * Frames that can be fragmented (only HEVC for now) require some preprocessing before they're returned.
         *
         * When a frame with an S-bit set is received, the frame is saved to "frames" array and an NTP timestamp
         * is saved. All subsequent frag frames are also saved in the "frames" array and they may arrive in
         * any order they want because they're saved in the array using the sequence number.
         *
         * Each time a new frame is received, the initial timestamp is compared with current time to see
         * how much time this frame still has left until it's discarded. Each frame is given N milliseconds
         * and if all its fragments are not received within that time window, the frame is considered invalid
         * and all fragments are discarded.
         *
         * When the last fragment is received (ie. the frame with E-bit set) AND if all previous fragments
         * very received too, frame_receiver() will call post-processing function to merge all the fragments
         * into one complete frame.
         *
         * If all previous fragments have not been received (ie. some frame is late), the code will wait
         * until the time windows closes. When that happens, the array is inspected once more to see if
         * all fragments were received and if so, the fragments are merged and returned to user.
         *
         * If some fragments were dropped, the whole HEVC frame is discarded
         *
         * Due to the nature of UDP, it's possible that during a fragment reception,
         * a stray RTP packet from earlier fragment might be received and that might corrupt
         * the reception process. To mitigate this, the sequence number and RTP timestamp of each
         * incoming packet is matched and checked against our own clock to get sense whether this packet valid
         *
         * Invalid packets (such as very late packets) are discarded automatically without further processing */
        const size_t HEVC_HDR_SIZE =
            kvz_rtp::frame::HEADER_SIZE_HEVC_NAL +
            kvz_rtp::frame::HEADER_SIZE_HEVC_FU;

        int type = __check_frame(frame);

        if (type == FT_NOT_FRAG) {
            reader->return_frame(frame);
            continue;
        }

        if (type == FT_INVALID) {
            LOG_WARN("invalid frame received!");
            (void)kvz_rtp::frame::dealloc_frame(frame);
            break;
        }

        /* TODO: this is ugly */
        bool duplicate = true;

        /* Save the received frame to "frames" array where frames are indexed using
         * their sequence number. This way when all fragments of a frame are received,
         * we can loop through the range sframe_seq - eframe_seq and merge all fragments */
        if (frames[frame->header.seq] == nullptr) {
            frames[frame->header.seq] = frame;
            duplicate                 = false;
        }

        /* If this is the first packet received with this timestamp, create new entry
         * to s_timers and save current time.
         *
         * This timestamp is used to keep track of how long we've been receiving chunks
         * and if the time exceeds RTP_FRAME_MAX_DELAY, we drop the frame */
        if (s_timers.find(frame->header.timestamp) == s_timers.end()) {
            /* UDP being unreliable, we can't know for sure in what order the packets are arriving.
             * Especially on linux where the fragment frames are batched and sent together it possible
             * that the first fragment we receive is the fragment containing the E-bit which sounds weird
             *
             * When the first fragment is received (regardless of its type), the timer is started and if the
             * fragment is special (S or E bit set), the sequence number is saved so we know the range of complete
             * full HEVC frame if/when all fragments have been received */

            if (type == FT_START) {
                s_timers[frame->header.timestamp].sframe_seq = frame->header.seq;
                s_timers[frame->header.timestamp].eframe_seq = INVALID_SEQ;
            } else if (type == FT_END) {
                s_timers[frame->header.timestamp].eframe_seq = frame->header.seq;
                s_timers[frame->header.timestamp].sframe_seq = INVALID_SEQ;
            } else {
                s_timers[frame->header.timestamp].sframe_seq = INVALID_SEQ;
                s_timers[frame->header.timestamp].eframe_seq = INVALID_SEQ;
            }

            s_timers[frame->header.timestamp].sframe_time   = kvz_rtp::clock::hrc::now();
            s_timers[frame->header.timestamp].total_size    = frame->payload_len - HEVC_HDR_SIZE;
            s_timers[frame->header.timestamp].pkts_received = 1;
            continue;
        }

        uint64_t diff = kvz_rtp::clock::hrc::diff_now(s_timers[frame->header.timestamp].sframe_time);

        if (diff > RTP_FRAME_MAX_DELAY) {
            if (dropped_frames.find(frame->header.timestamp) == dropped_frames.end()) {
                dropped_frames[frame->header.timestamp] = 1;
            } else {
                dropped_frames[frame->header.timestamp]++;
            }

            frames[frame->header.seq] = nullptr;
            (void)kvz_rtp::frame::dealloc_frame(frame);
            continue;
        }

        if (!duplicate) {
            s_timers[frame->header.timestamp].pkts_received++;
            s_timers[frame->header.timestamp].total_size += (frame->payload_len - HEVC_HDR_SIZE);
        }

        if (type == FT_START)
            s_timers[frame->header.timestamp].sframe_seq = frame->header.seq;

        if (type == FT_END)
            s_timers[frame->header.timestamp].eframe_seq = frame->header.seq;

        if (s_timers[frame->header.timestamp].sframe_seq != INVALID_SEQ &&
            s_timers[frame->header.timestamp].eframe_seq != INVALID_SEQ)
        {
            uint32_t ts    = frame->header.timestamp;
            uint16_t s_seq = s_timers[ts].sframe_seq;
            uint16_t e_seq = s_timers[ts].eframe_seq;
            size_t ptr     = 0;

            /* we've received every fragment and the frame can be reconstructed */
            if (e_seq - s_seq + 1 == (ssize_t)s_timers[frame->header.timestamp].pkts_received) {
                nal_header[0] = (frames[s_seq]->payload[0] & 0x81) | ((frame->payload[2] & 0x3f) << 1);
                nal_header[1] =  frames[s_seq]->payload[1];

                kvz_rtp::frame::rtp_frame *out = kvz_rtp::frame::alloc_rtp_frame();

                out->payload_len = s_timers[frame->header.timestamp].total_size + kvz_rtp::frame::HEADER_SIZE_HEVC_NAL;
                out->payload     = new uint8_t[out->payload_len];

                std::memcpy(&out->header,  &frames[s_seq]->header, kvz_rtp::frame::HEADER_SIZE_RTP);
                std::memcpy(out->payload,  nal_header,             kvz_rtp::frame::HEADER_SIZE_HEVC_NAL);

                ptr += kvz_rtp::frame::HEADER_SIZE_HEVC_NAL;

                for (size_t i = s_seq; i <= e_seq; ++i) {
                    std::memcpy(
                        &out->payload[ptr],
                        &frames[i]->payload[HEVC_HDR_SIZE],
                        frames[i]->payload_len - HEVC_HDR_SIZE
                    );
                    ptr += frames[i]->payload_len - HEVC_HDR_SIZE;
                    (void)kvz_rtp::frame::dealloc_frame(frames[i]);
                    frames[i] = nullptr;
                }

                reader->return_frame(out);
                s_timers.erase(ts);
            }
        }
    }

    return ret;
}
