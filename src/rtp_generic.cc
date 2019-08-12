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
#include "reader.hh"
#include "rtp_generic.hh"
#include "send.hh"
#include "util.hh"
#include "writer.hh"

// TODO implement frame splitting if data_len > MTU
rtp_error_t kvz_rtp::generic::push_frame(kvz_rtp::connection *conn, uint8_t *data, size_t data_len, uint32_t timestamp)
{
    if (data_len > MAX_PAYLOAD) {
        LOG_WARN("packet is larger (%zu bytes) than MAX_PAYLOAD (%u bytes)", data_len, MAX_PAYLOAD);
    }

    uint8_t header[kvz_rtp::frame::HEADER_SIZE_RTP];
    conn->fill_rtp_header(header, timestamp);

    return kvz_rtp::send::send_frame(conn, header, sizeof(header), data, data_len);
}

rtp_error_t kvz_rtp::generic::frame_receiver(kvz_rtp::reader *reader)
{
    LOG_INFO("Generic frame receiver starting...");

    /* TODO: this looks super ugly */
    rtp_error_t ret;
    int nread = 0, ftype = 0;
    sockaddr_in sender_addr;
    kvz_rtp::socket socket = reader->get_socket();
    kvz_rtp::frame::rtp_frame *frame, *f, *frames[0xffff + 1] = { 0 };

    uint8_t nal_header[2] = { 0 };
    std::map<uint32_t, size_t> dropped_frames;
    size_t n_dropped_frames = 0, n_new_frames = 0, cached = 0;

    while (reader->active()) {
        ret = socket.recvfrom(reader->get_recv_buffer(), reader->get_recv_buffer_len(), 0, &sender_addr, &nread);

        if (ret != RTP_OK) {
            LOG_ERROR("recvfrom failed! FrameReceiver cannot continue!");
            return RTP_GENERIC_ERROR;
        }

        if ((frame = reader->validate_rtp_frame(reader->get_recv_buffer(), nread)) == nullptr) {
            LOG_DEBUG("received an invalid frame, discarding");
            continue;
        }

        /* Update session related statistics
         * If this is a new peer, RTCP will take care of initializing necessary stuff */
        reader->update_receiver_stats(frame);
        reader->return_frame(frame);
    }

    return ret;
}
