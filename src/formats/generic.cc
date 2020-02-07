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
#include "conn.hh"
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

    /* return kvz_rtp::send::send_frame(conn, header, sizeof(header), data, data_len); */
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
    /* return kvz_rtp::send::send_frame(conn, header, sizeof(header), data.get(), data_len); */
}

rtp_error_t kvz_rtp::generic::frame_receiver(kvz_rtp::receiver *receiver)
{
    LOG_INFO("Generic frame receiver starting...");

    /* TODO: this looks super ugly */
    rtp_error_t ret;
    int nread = 0;
    sockaddr_in sender_addr;
    kvz_rtp::socket socket = receiver->get_socket();
    kvz_rtp::frame::rtp_frame *frame;

    while (receiver->active()) {
        ret = socket.recvfrom(receiver->get_recv_buffer(), receiver->get_recv_buffer_len(), 0, &sender_addr, &nread);

        if (ret != RTP_OK) {
            LOG_ERROR("recvfrom failed! FrameReceiver cannot continue!");
            return RTP_GENERIC_ERROR;
        }

        if ((frame = receiver->validate_rtp_frame(receiver->get_recv_buffer(), nread)) == nullptr) {
            LOG_DEBUG("received an invalid frame, discarding");
            continue;
        }
        memcpy(&frame->src_addr, &sender_addr, sizeof(sockaddr_in));

        /* Update session related statistics
         * If this is a new peer, RTCP will take care of initializing necessary stuff */
        /* if (receiver->update_receiver_stats(frame) == RTP_OK) */
        /*     receiver->return_frame(frame); */
    }

    return ret;
}
