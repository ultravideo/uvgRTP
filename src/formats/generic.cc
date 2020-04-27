#ifdef _WIN32
// TODO
#else
#include <arpa/inet.h>
#endif

#include <cstdint>
#include <cstring>
#include <iostream>
#include <map>

#include "debug.hh"
#include "receiver.hh"
#include "send.hh"
#include "sender.hh"
#include "util.hh"

#include "formats/generic.hh"

#define INVALID_SEQ 0xffffffff

/* The generic frames are fragmented using the marker bit of the RTP header.
 * First and last fragment of a larger frame have marker bits set and middle fragments don't.
 * All fragments have the same timestamp so the receiver knows which fragments are part of a larger frame. */
rtp_error_t uvg_rtp::generic::push_frame(uvg_rtp::sender *sender, uint8_t *data, size_t data_len, int flags)
{
    (void)flags;

    uint8_t header[uvg_rtp::frame::HEADER_SIZE_RTP];
    sender->get_rtp_ctx()->fill_header(header);

    if (data_len > MAX_PAYLOAD) {
        if (sender->get_conf().flags & RCE_FRAGMENT_GENERIC) {

            rtp_error_t ret   = RTP_OK;
            ssize_t data_left = data_len;
            ssize_t data_pos  = 0;

            /* set marker bit for the first fragment */
            header[1] |= (1 << 7);

            while (data_left > MAX_PAYLOAD) {
                ret = uvg_rtp::send::send_frame(sender, header, sizeof(header), data + data_pos, MAX_PAYLOAD);

                if (ret != RTP_OK)
                    return ret;

                sender->get_rtp_ctx()->update_sequence(header);

                data_pos  += MAX_PAYLOAD;
                data_left -= MAX_PAYLOAD;

                /* clear marker bit for middle fragments */
                header[1] &= 0x7f;
            }

            /* set marker bit for the last frame */
            header[1] |= (1 << 7);
            return uvg_rtp::send::send_frame(sender, header, sizeof(header), data + data_pos, data_left);

        } else {
            LOG_WARN("packet is larger (%zu bytes) than MAX_PAYLOAD (%u bytes)", data_len, MAX_PAYLOAD);
        }
    }

    return uvg_rtp::send::send_frame(sender, header, sizeof(header), data, data_len);
}

rtp_error_t uvg_rtp::generic::push_frame(uvg_rtp::sender *sender, std::unique_ptr<uint8_t[]> data, size_t data_len, int flags)
{
    return uvg_rtp::generic::push_frame(sender, data.get(), data_len, flags);
}

static rtp_error_t __fragment_receiver(uvg_rtp::receiver *receiver)
{
    LOG_INFO("use fragment receiver");

    int nread = 0;
    sockaddr_in sender_addr;
    rtp_error_t ret = RTP_OK;
    uvg_rtp::socket socket = receiver->get_socket();
    uvg_rtp::frame::rtp_frame *frame;

    struct frame_info {
        uint32_t s_seq;
        uint32_t e_seq;
        size_t npkts;
        std::map<uint16_t, uvg_rtp::frame::rtp_frame *> fragments;
    };
    std::unordered_map<uint32_t, frame_info> frames;

    fd_set read_fds;
    struct timeval t_val;

    FD_ZERO(&read_fds);

    t_val.tv_sec  = 0;
    t_val.tv_usec = 1500;

    while (!receiver->active())
        ;

    while (receiver->active()) {
        FD_SET(socket.get_raw_socket(), &read_fds);
        int sret = ::select(socket.get_raw_socket() + 1, &read_fds, nullptr, nullptr, &t_val);

        if (sret < 0) {
#ifdef __linux__
            LOG_ERROR("select failed: %s!", strerror(errno));
#else
            win_get_last_error();
#endif
            return RTP_GENERIC_ERROR;
        }

        do {
#ifdef __linux__
            ret = socket.recvfrom(receiver->get_recv_buffer(), receiver->get_recv_buffer_len(), MSG_DONTWAIT, &sender_addr, &nread);

            if (ret != RTP_OK) {
                if (errno == EAGAIN || errno == EWOULDBLOCK)
                    break;

                LOG_ERROR("recvfrom failed! FrameReceiver cannot continue %s!", strerror(errno));
                return RTP_GENERIC_ERROR;
            }
#else
            ret = socket.recvfrom(receiver->get_recv_buffer(), receiver->get_recv_buffer_len(), 0, &sender_addr, &nread);

            if (ret != RTP_OK) {
                if (WSAGetLastError() == WSAEWOULDBLOCK)
                    break;

                LOG_ERROR("recvfrom failed! FrameReceiver cannot continue %s!", strerror(errno));
                return RTP_GENERIC_ERROR;
            }
#endif

            if ((frame = receiver->validate_rtp_frame(receiver->get_recv_buffer(), nread)) == nullptr) {
                LOG_DEBUG("received an invalid frame, discarding");
                continue;
            }
            memcpy(&frame->src_addr, &sender_addr, sizeof(sockaddr_in));

            uint32_t ts  = frame->header.timestamp;
            uint32_t seq = frame->header.seq;
            size_t recv  = 0;

            if (frames.find(ts) != frames.end()) {
                frames[ts].npkts++;
                frames[ts].fragments[seq] = frame;

                if (frame->header.marker)
                    frames[ts].e_seq = seq;

                if (frames[ts].e_seq != INVALID_SEQ && frames[ts].s_seq != INVALID_SEQ) {
                    if (frames[ts].s_seq > frames[ts].e_seq)
                        recv = 0xffff - frames[ts].s_seq + frames[ts].e_seq + 2;
                    else
                        recv = frames[ts].e_seq - frames[ts].s_seq + 1;

                    if (recv == frames[ts].npkts) {
                        auto retframe = uvg_rtp::frame::alloc_rtp_frame(recv * MAX_PAYLOAD);
                        size_t ptr    = 0;

                        std::memcpy(&retframe->header, &frame->header, sizeof(frame->header));

                        for (auto& frag : frames[ts].fragments) {
                            std::memcpy(
                                retframe->payload + ptr,
                                frag.second->payload,
                                frag.second->payload_len
                            );
                            ptr += frag.second->payload_len;
                            (void)uvg_rtp::frame::dealloc_frame(frag.second);
                        }

                        frames.erase(ts);
                        receiver->return_frame(retframe);
                    }
                }
            } else {
                if (frame->header.marker) {
                    frames[ts].npkts          = 1;
                    frames[ts].s_seq          = seq;
                    frames[ts].e_seq          = INVALID_SEQ;
                    frames[ts].fragments[seq] = frame;
                } else {
                    receiver->return_frame(frame);
                }
            }
        } while (ret == RTP_OK);
    }

    receiver->get_mutex().unlock();
    return ret;
}

rtp_error_t uvg_rtp::generic::frame_receiver(uvg_rtp::receiver *receiver)
{
    /* If user uses fragmented generic frames, start the fragment receiver instead */
    if (receiver->get_conf().flags & RCE_FRAGMENT_GENERIC)
        return __fragment_receiver(receiver);

    int nread = 0;
    sockaddr_in sender_addr;
    rtp_error_t ret = RTP_OK;
    uvg_rtp::socket socket = receiver->get_socket();
    uvg_rtp::frame::rtp_frame *frame;

    fd_set read_fds;
    struct timeval t_val;

    FD_ZERO(&read_fds);

    t_val.tv_sec  = 0;
    t_val.tv_usec = 1500;

    while (!receiver->active())
        ;

    while (receiver->active()) {
        FD_SET(socket.get_raw_socket(), &read_fds);
        int sret = ::select(socket.get_raw_socket() + 1, &read_fds, nullptr, nullptr, &t_val);

        if (sret < 0) {
#ifdef __linux__
            LOG_ERROR("select failed: %s!", strerror(errno));
#else
            win_get_last_error();
#endif
            return RTP_GENERIC_ERROR;
        }

        do {
#ifdef __linux__
            ret = socket.recvfrom(receiver->get_recv_buffer(), receiver->get_recv_buffer_len(), MSG_DONTWAIT, &sender_addr, &nread);

            if (ret != RTP_OK) {
                if (errno == EAGAIN || errno == EWOULDBLOCK)
                    break;

                LOG_ERROR("recvfrom failed! FrameReceiver cannot continue %s!", strerror(errno));
                return RTP_GENERIC_ERROR;
            }
#else
            ret = socket.recvfrom(receiver->get_recv_buffer(), receiver->get_recv_buffer_len(), 0, &sender_addr, &nread);

            if (ret != RTP_OK) {
                if (WSAGetLastError() == WSAEWOULDBLOCK)
                    break;

                LOG_ERROR("recvfrom failed! FrameReceiver cannot continue %s!", strerror(errno));
                return RTP_GENERIC_ERROR;
            }
#endif


            if ((frame = receiver->validate_rtp_frame(receiver->get_recv_buffer(), nread)) == nullptr) {
                LOG_DEBUG("received an invalid frame, discarding");
                continue;
            }
            memcpy(&frame->src_addr, &sender_addr, sizeof(sockaddr_in));

            /* Update session related statistics
             * If this is a new peer, RTCP will take care of initializing necessary stuff */
            /* if (receiver->update_receiver_stats(frame) == RTP_OK) */
                receiver->return_frame(frame);
        } while (ret == RTP_OK);
    }

    receiver->get_mutex().unlock();
    return ret;
}
