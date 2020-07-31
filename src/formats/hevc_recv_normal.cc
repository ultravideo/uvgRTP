#include <cstdint>
#include <cstring>
#include <iostream>
#include <map>
#include <unordered_set>

#include "debug.hh"
#include "queue.hh"
#include "receiver.hh"
#include "send.hh"

#define RTP_FRAME_MAX_DELAY          100
#define INVALID_SEQ           0x13371338
#define INVALID_TS            0xffffffff

#define RTP_HDR_SIZE  12
#define NAL_HDR_SIZE   2

enum FRAG_TYPES {
    FT_INVALID   = -2, /* invalid combination of S and E bits */
    FT_NOT_FRAG  = -1, /* frame doesn't contain HEVC fragment */
    FT_START     =  1, /* frame contains a fragment with S bit set */
    FT_MIDDLE    =  2, /* frame is fragment but not S or E fragment */
    FT_END       =  3, /* frame contains a fragment with E bit set */
};

enum NAL_TYPES {
    NT_INTRA = 0x00,
    NT_INTER = 0x01,
    NT_OTHER = 0xff
};

typedef std::unordered_map<uint32_t, struct hevc_info> frame_info_t;

struct hevc_info {
    /* clock reading when the first fragment is received */
    uvg_rtp::clock::hrc::hrc_t sframe_time;

    /* sequence number of the frame with s-bit */
    uint32_t s_seq;

    /* sequence number of the frame with e-bit */
    uint32_t e_seq;

    /* how many fragments have been received */
    size_t pkts_received;

    /* total size of all fragments */
    size_t total_size;

    /* map of frame's fragments,
     * allows out-of-order insertion and loop-through in order */
    std::map<uint16_t, uvg_rtp::frame::rtp_frame *> fragments;
};

static int __get_frag(uvg_rtp::frame::rtp_frame *frame)
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

static inline uint8_t __get_nal(uvg_rtp::frame::rtp_frame *frame)
{
    switch (frame->payload[2] & 0x3f) {
        case 19: return NT_INTRA;
        case 1:  return NT_INTER;
        default: break;
    }

    return NT_OTHER;
}

static inline bool __frame_late(hevc_info& hinfo)
{
    return (uvg_rtp::clock::hrc::diff_now(hinfo.sframe_time) >= RTP_FRAME_MAX_DELAY);
}

static void __drop_frame(frame_info_t& finfo, uint32_t ts)
{
    uint16_t s_seq = finfo.at(ts).s_seq;
    uint16_t e_seq = finfo.at(ts).e_seq;

    LOG_INFO("Dropping frame %u, %u - %u", ts, s_seq, e_seq);

    for (auto& fragment : finfo.at(ts).fragments)
        (void)uvg_rtp::frame::dealloc_frame(fragment.second);

    finfo.erase(ts);
}

rtp_error_t __hevc_receiver(uvg_rtp::receiver *receiver)
{
    int nread = 0;
    frame_info_t finfo;
    rtp_error_t ret = RTP_OK;
    uvg_rtp::frame::rtp_frame *frame;
    uvg_rtp::socket socket = receiver->get_socket();
    bool enable_idelay = !(receiver->get_conf().flags & RCE_HEVC_NO_INTRA_DELAY);
    std::unordered_set<uint32_t> dropped;

    /* Use "intra" to keep track of intra frames
     *
     * If uvgRTP is in the process of receiving fragments of an incomplete intra frame,
     * "intra" shall be the timestamp value of that intra frame.
     * This means that when we're receiving packets out of order and an inter frame is complete
     * while "intra" contains value other than INVALID_TS, we drop the inter frame and wait for
     * the intra frame to complete.
     *
     * If "intra" contains INVALID_TS and all packets of an inter frame have been received,
     * the inter frame is returned to user.  If intra contains a value other than INVALID_TS
     * (meaning an intra frame is in progress) and a new intra frame is received, the old intra frame
     * pointed to by "intra" and new intra frame shall take the place of active intra frame */
    uint32_t intra  = INVALID_TS;

    fd_set read_fds;
    struct timeval t_val;
    FD_ZERO(&read_fds);

    while (!receiver->active())
        ;

    while (receiver->active()) {
        /* Reset select() parameters.
         *
         * FD_SET() must be called every time before calling select() at least on Windows
         * and on Linux, select() changes the timeout values of t_val to relect the time waited
         * on the file descriptors */
        FD_SET(socket.get_raw_socket(), &read_fds);
        t_val = { 0, 1000 };

        int sret = ::select(socket.get_raw_socket() + 1, &read_fds, nullptr, nullptr, &t_val);

        if (sret < 0) {
            log_platform_error("select(2) failed");
            return RTP_GENERIC_ERROR;
        }

        do {
#ifdef __linux__
            ret = socket.recvfrom(receiver->get_recv_buffer(), receiver->get_recv_buffer_len(), MSG_DONTWAIT, nullptr, &nread);

            if (ret != RTP_OK) {
                if (errno == EAGAIN || errno == EWOULDBLOCK)
                    break;

                LOG_ERROR("recvfrom failed! FrameReceiver cannot continue %s!", strerror(errno));
                return RTP_GENERIC_ERROR;
            }
#else
            ret = socket.recvfrom(receiver->get_recv_buffer(), receiver->get_recv_buffer_len(), 0, nullptr, &nread);

            if (ret != RTP_OK) {
                if (WSAGetLastError() == WSAEWOULDBLOCK)
                    break;

                LOG_ERROR("recvfrom failed! FrameReceiver cannot continue %s!", strerror(errno));
                return RTP_GENERIC_ERROR;
            }
#endif

            /* Frame might be actually invalid or it might be a ZRTP frame, just continue to next frame */
            if ((frame = receiver->validate_rtp_frame(receiver->get_recv_buffer(), nread)) == nullptr)
                continue;

            /* Update session related statistics
             * If this is a new peer, RTCP will take care of initializing necessary stuff
             *
             * Skip processing the packet if it was invalid. This is mostly likely caused
             * by an SSRC collision */
            if (receiver->update_receiver_stats(frame) != RTP_OK)
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
                uvg_rtp::frame::HEADER_SIZE_HEVC_NAL +
                uvg_rtp::frame::HEADER_SIZE_HEVC_FU;

            uint32_t c_ts    = frame->header.timestamp;
            uint32_t c_seq   = frame->header.seq;
            int frag_type    = __get_frag(frame);
            uint8_t nal_type = __get_nal(frame);

            if (frag_type == FT_NOT_FRAG) {
                receiver->return_frame(frame);
                continue;
            }

            if (frag_type == FT_INVALID) {
                LOG_WARN("invalid frame received!");
                (void)uvg_rtp::frame::dealloc_frame(frame);
                continue;
            }

            /* initialize new frame */
            if (finfo.find(c_ts) == finfo.end()) {

                /* make sure we haven't discarded the frame "c_ts" before */
                if (dropped.find(c_ts) != dropped.end()) {
                    LOG_WARN("packet belonging to a dropped frame was received!");
                    continue;
                }

                /* drop old intra if a new one is received */
                if (nal_type == NT_INTRA) {
                    if (intra != INVALID_TS && enable_idelay) {
                        __drop_frame(finfo, intra);
                        dropped.insert(intra);
                    }
                    intra = c_ts;
                }

                finfo[c_ts].s_seq = INVALID_SEQ;
                finfo[c_ts].e_seq = INVALID_SEQ;

                if (frag_type == FT_START) finfo[c_ts].s_seq = c_seq;
                if (frag_type == FT_END)   finfo[c_ts].e_seq = c_seq;

                finfo[c_ts].sframe_time   = uvg_rtp::clock::hrc::now();
                finfo[c_ts].total_size    = frame->payload_len - HEVC_HDR_SIZE;
                finfo[c_ts].pkts_received = 1;

                finfo[c_ts].fragments[c_seq] = frame;
                continue;
            }
            finfo[c_ts].fragments[c_seq] = frame;

            finfo[c_ts].pkts_received += 1;
            finfo[c_ts].total_size    += (frame->payload_len - HEVC_HDR_SIZE);

            if (frag_type == FT_START)
                finfo[c_ts].s_seq = c_seq;

            if (frag_type == FT_END)
                finfo[c_ts].e_seq = c_seq;

            if (finfo[c_ts].s_seq != INVALID_SEQ && finfo[c_ts].e_seq != INVALID_SEQ) {
                size_t received = 0;
                size_t fptr     = 0;
                size_t s_seq    = finfo[c_ts].s_seq;
                size_t e_seq    = finfo[c_ts].e_seq;

                if (s_seq > e_seq)
                    received = 0xffff - s_seq + e_seq + 2;
                else
                    received = e_seq - s_seq + 1;

                /* we've received every fragment and the frame can be reconstructed */
                if (received == finfo[c_ts].pkts_received) {

                    /* intra is still in progress, do not return the inter */
                    if (nal_type == NT_INTER && intra != INVALID_TS && enable_idelay) {
                        __drop_frame(finfo, c_ts);
                        dropped.insert(c_ts);
                        continue;
                    }

                    uint8_t nal_header[2] = {
                        (uint8_t)((frame->payload[0] & 0x81) | ((frame->payload[2] & 0x3f) << 1)),
                        (uint8_t)frame->payload[1]
                    };
                    uvg_rtp::frame::rtp_frame *out = uvg_rtp::frame::alloc_rtp_frame();

                    out->payload_len = finfo[c_ts].total_size + uvg_rtp::frame::HEADER_SIZE_HEVC_NAL;
                    out->payload     = new uint8_t[out->payload_len];

                    std::memcpy(&out->header,  &frame->header, RTP_HDR_SIZE);
                    std::memcpy(out->payload,  nal_header,     NAL_HDR_SIZE);

                    fptr += uvg_rtp::frame::HEADER_SIZE_HEVC_NAL;

                    for (auto& fragment : finfo.at(c_ts).fragments) {
                        std::memcpy(
                            &out->payload[fptr],
                            &fragment.second->payload[HEVC_HDR_SIZE],
                            fragment.second->payload_len - HEVC_HDR_SIZE
                        );
                        fptr += fragment.second->payload_len - HEVC_HDR_SIZE;
                        (void)uvg_rtp::frame::dealloc_frame(fragment.second);
                    }

                    if (nal_type == NT_INTRA)
                        intra = INVALID_TS;

                    receiver->return_frame(out);
                    finfo.erase(c_ts);
                    continue;
                }
            }

            if (__frame_late(finfo.at(c_ts))) {
                if (nal_type != NT_INTRA || (nal_type == NT_INTRA && !enable_idelay)) {
                    __drop_frame(finfo, c_ts);
                    dropped.insert(c_ts);
                }
            }
        } while (ret == RTP_OK);
    }

    receiver->get_mutex().unlock();
    return ret;
}
