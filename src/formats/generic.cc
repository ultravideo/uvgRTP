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

// TODO implement frame splitting if data_len > MTU
rtp_error_t kvz_rtp::generic::push_frame(kvz_rtp::sender *sender, uint8_t *data, size_t data_len, int flags)
{
    (void)flags;

    if (data_len > MAX_PAYLOAD) {
        LOG_WARN("packet is larger (%zu bytes) than MAX_PAYLOAD (%u bytes)", data_len, MAX_PAYLOAD);
    }

    uint8_t header[kvz_rtp::frame::HEADER_SIZE_RTP];
    sender->get_rtp_ctx()->fill_header(header);

    return kvz_rtp::send::send_frame(sender, header, sizeof(header), data, data_len);
}

rtp_error_t kvz_rtp::generic::push_frame(kvz_rtp::sender *sender, std::unique_ptr<uint8_t[]> data, size_t data_len, int flags)
{
    (void)flags;

    if (data_len > MAX_PAYLOAD) {
        LOG_WARN("packet is larger (%zu bytes) than MAX_PAYLOAD (%u bytes)", data_len, MAX_PAYLOAD);
    }

    uint8_t header[kvz_rtp::frame::HEADER_SIZE_RTP];
    sender->get_rtp_ctx()->fill_header(header);

    /* We don't need to transfer the ownership of of "data" to socket because send_frame() executes immediately */
    return kvz_rtp::send::send_frame(sender, header, sizeof(header), data.get(), data_len);
}

rtp_error_t kvz_rtp::generic::frame_receiver(kvz_rtp::receiver *receiver)
{
    LOG_INFO("Generic frame receiver starting...");

    int nread = 0;
    sockaddr_in sender_addr;
    rtp_error_t ret = RTP_OK;
    kvz_rtp::socket socket = receiver->get_socket();
    kvz_rtp::frame::rtp_frame *frame;

    fd_set read_fds;
    struct timeval t_val;

    FD_ZERO(&read_fds);
    FD_SET(socket.get_raw_socket(), &read_fds);

    t_val.tv_sec  = 0;
    t_val.tv_usec = 1500;

    while (receiver->active()) {
        int sret = ::select(1, &read_fds, nullptr, nullptr, &t_val);

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

    return ret;
}
