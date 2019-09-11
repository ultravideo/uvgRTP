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

#define PTR_DIFF(a, b)  ((ptrdiff_t)((char *)(a) - (char *)(b)))

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

#define haszero64_le(v) (((v) - 0x0101010101010101) & ~(v) & 0x8080808080808080UL)
#define haszero32_le(v) (((v) - 0x01010101)         & ~(v) & 0x80808080UL)

#define haszero64_be(v) (((v) - 0x1010101010101010) & ~(v) & 0x0808080808080808UL)
#define haszero32_be(v) (((v) - 0x10101010)         & ~(v) & 0x08080808UL)

static inline unsigned __find_hevc_start(uint32_t value)
{
#if __BYTE_ORDER == __LITTLE_ENDIAN
    uint16_t u = (value >> 16) & 0xffff;
    uint16_t l = (value >>  0) & 0xffff;

    bool t1 = (l == 0);
    bool t2 = ((u & 0xff) == 0x01);
    bool t3 = (u == 0x0100);
    bool t4 = (((l >> 8) & 0xff) == 0);
#else
    uint16_t u = (value >>  0) & 0xffff;
    uint16_t l = (value >> 16) & 0xffff;

    bool t1 = (l == 0);
    bool t2 = (((u >> 8) & 0xff) == 0x01);
    bool t3 = (u == 0x0001);
    bool t4 = ((l & 0xff) == 0);
#endif

    if (t1) {
        /* 0x00000001 */
        if (t3)
            return 4;

        /* "value" definitely has a start code (0x000001XX), but at this
         * point we can't know for sure whether it's 3 or 4 bytes long.
         *
         * Return 5 to indicate that start length could not be determined
         * and that caller must check previous dword's last byte for 0x00 */
        if (t2)
            return 5;
    } else if (t4 && t3) {
        /* 0xXX000001 */
        return 4;
    }

    return 0;
}

/* NOTE: the area 0 - len (ie data[0] - data[len - 1]) must be addressable!
 * Do not add offset to "data" ptr before passing it to __get_hevc_start()! */
static ssize_t __get_hevc_start(uint8_t *data, size_t len, size_t offset, uint8_t& start_len)
{
    int found     = 0;
    bool prev_z   = false;
    bool cur_z    = false;
    size_t pos    = offset;
    uint8_t *ptr  = data + offset;
    uint8_t *tmp  = nullptr;
    uint8_t lb    = 0;
    uint32_t prev = UINT32_MAX;

    uint64_t prefetch = UINT64_MAX;
    uint32_t value    = UINT32_MAX;
    unsigned ret      = 0;

    /* We can get rid of the bounds check when looping through
     * non-zero 8 byte chunks by setting the last byte to zero.
     *
     * This added zero will make the last 8 byte zero check to fail
     * and when we get out of the loop we can check if we've reached the end */
    lb = data[len - 1];
    data[len - 1] = 0;

    while (pos < len) {
        prefetch = *(uint64_t *)ptr;

#if __BYTE_ORDER == __LITTLE_ENDIAN
        if (!prev_z && !(cur_z = haszero64_le(prefetch))) {
#else
        if (!prev_z && !(cur_z = haszero64_be(prefetch))) {
#endif
            /* pos is not used in the following loop so it makes little sense to
             * update it on every iteration. Faster way to do the loop is to save
             * ptr's current value before loop, update only ptr in the loop and when
             * the loop is exited, calculate the difference between tmp and ptr to get
             * the number of iterations done * 8 */
            tmp = ptr;

            do {
                ptr      += 8;
                prefetch  = *(uint64_t *)ptr;
#if __BYTE_ORDER == __LITTLE_ENDIAN
                cur_z     = haszero64_le(prefetch);
#else
                cur_z     = haszero64_be(prefetch);
#endif
            } while (!cur_z);

            pos += PTR_DIFF(ptr, tmp);

            if (pos >= len)
                break;
        }

        value = *(uint32_t *)ptr;

        if (cur_z)
#if __BYTE_ORDER == __LITTLE_ENDIAN
            cur_z = haszero32_le(value);
#else
            cur_z = haszero32_be(value);
#endif

        if (!prev_z && !cur_z)
            goto end;

        /* Previous dword had zeros but this doesn't. The only way there might be a start code
         * is if the most significant byte of current dword is 0x01 */
        if (prev_z && !cur_z) {
#if __BYTE_ORDER == __LITTLE_ENDIAN
            /* previous dword: 0xXX000000 or 0xXXXX0000 and current dword 0x01XXXXXX */
            if (((value  >> 0) & 0xff) == 0x01 && ((prev >> 16) & 0xffff) == 0) {
                start_len = (((prev >>  8) & 0xffffff) == 0) ? 4 : 3;
#else
            if (((value >> 24) & 0xff) == 0x01 && ((prev >>  0) & 0xffff) == 0) {
                start_len = (((prev >>  0) & 0xffffff) == 0) ? 4 : 3;
#endif
                return pos + 1;
            }
        }


        {
            if ((ret = start_len = __find_hevc_start(value)) > 0) {
                if (ret == 5) {
                    ret = 3;
#if __BYTE_ORDER == __LITTLE_ENDIAN
                    start_len = (((prev >> 24) & 0xff) == 0) ? 4 : 3;
#else
                    start_len = (((prev >>  0) & 0xff) == 0) ? 4 : 3;
#endif
                }

                return pos + ret;
            }

#if __BYTE_ORDER == __LITTLE_ENDIAN
            uint16_t u = (value >> 16) & 0xffff;
            uint16_t l = (value >>  0) & 0xffff;
            uint16_t p = (prev  >> 16) & 0xffff;

            bool t1 = ((p & 0xffff) == 0);
            bool t2 = (((p >> 8) & 0xff) == 0);
            bool t4 = (l == 0x0100);
            bool t5 = (l == 0x0000 && u == 0x01);
#else
            uint16_t u = (value >>  0) & 0xffff;
            uint16_t l = (value >> 16) & 0xffff;
            uint16_t p = (prev  >>  0) & 0xffff;

            bool t1 = ((p & 0xffff) == 0);
            bool t2 = ((p & 0xff) == 0);
            bool t4 = (l == 0x0001);
            bool t5 = (l == 0x0000 && u == 0x01);
#endif
            if (t1 && t4) {
                /* previous dword 0xxxxx0000 and current dword is 0x0001XXXX */
                if (t4) {
                    start_len = 4;
                    return pos + 2;
                }
            /* Previous dwod was 0xXXXXXX00 */
            } else if (t2) {
                /* Current dword is 0x000001XX */
                if (t5) {
                    start_len = 4;
                    return pos + 3;
                }

                /* Current dword is 0x0001XXXX */
                else if (t4) {
                    start_len = 3;
                    return pos + 2;
                }
            }

        }
end:
        prev_z = cur_z;
        pos += 4;
        ptr += 4;
        prev = value;
    }

    return -1;
}

static rtp_error_t __push_hevc_frame(
    kvz_rtp::connection *conn, kvz_rtp::frame_queue *fqueue,
    uint8_t *data, size_t data_len,
    bool more
)
{
    uint8_t nalType  = (data[0] >> 1) & 0x3F;
    rtp_error_t ret  = RTP_OK;
    size_t data_left = data_len;
    size_t data_pos  = 0;

#ifdef __linux__
    if (data_len <= MAX_PAYLOAD) {
        if ((ret = fqueue->enqueue_message(conn, data, data_len)) != RTP_OK)
            return ret;
        return more ? RTP_NOT_READY : RTP_OK;
    }

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
    if (data_len <= MAX_PAYLOAD) {
        LOG_DEBUG("send unfrag size %zu, type %u", data_len, nalType);
        return kvz_rtp::generic::push_frame(conn, data, data_len, 0);
    }

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

    return kvz_rtp::send::send_frame(conn, buffer, HEADER_SIZE + data_left);
#endif
}

rtp_error_t kvz_rtp::hevc::push_frame(kvz_rtp::connection *conn, uint8_t *data, size_t data_len, int flags)
{
    (void)flags;

#ifdef __linux__
    /* find first start code */
    uint8_t start_len = 0;
    int offset        = __get_hevc_start(data, data_len, 0, start_len);
    int prev_offset   = offset;
    size_t r_off      = 0;
    rtp_error_t ret   = RTP_GENERIC_ERROR;

    if (data_len < MAX_PAYLOAD) {
        r_off = (offset < 0) ? 0 : offset; /* TODO: this looks ugly */
        return kvz_rtp::generic::push_frame(conn, data + r_off, data_len - r_off, flags);
    }

    kvz_rtp::frame_queue *fqueue = conn->get_frame_queue();
    fqueue->init_queue(conn);

    while (offset != -1) {
        offset = __get_hevc_start(data, data_len, offset, start_len);

        if (offset != -1) {
            ret = __push_hevc_frame(conn, fqueue, &data[prev_offset], offset - prev_offset - start_len, true);

            if (ret != RTP_NOT_READY)
                goto error;

            prev_offset = offset;
        }
    }

    if ((ret = __push_hevc_frame(conn, fqueue, &data[prev_offset], data_len - prev_offset, false)) == RTP_OK)
        return RTP_OK;

error:
    fqueue->empty_queue();
    return ret;
#else
    uint8_t start_len;
    int32_t prev_offset = 0;
    int offset = __get_hevc_start(data, ata_len, 0, start_len);
    prev_offset = offset;

    while (offset != -1) {
        offset = __get_hevc_start(data, data_len, offset, start_len);

        if (offset > 4 && offset != -1) {
            if (__push_hevc_frame(conn, nullptr, &data[prev_offset], offset - prev_offset - start_len, false) == -1)
                return RTP_GENERIC_ERROR;

            prev_offset = offset;
        }
    }

    if (prev_offset == -1)
        prev_offset = 0;

    return __push_hevc_frame(conn, nullptr, &data[prev_offset], data_len - prev_offset, false);
#endif
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
