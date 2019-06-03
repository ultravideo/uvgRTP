#ifdef _WIN32
// TODO
#else
#include <arpa/inet.h>
#endif

#include <stdint.h>
#include <cstring>
#include <iostream>

#include "debug.hh"
#include "conn.hh"
#include "rtp_generic.hh"
#include "send.hh"
#include "util.hh"
#include "writer.hh"

// TODO implement frame splitting if dataLen > MTU
rtp_error_t kvz_rtp::generic::push_generic_frame(connection *conn, uint8_t *data, size_t data_len, uint32_t timestamp)
{
    rtp_error_t ret;

    if (data_len > MAX_PAYLOAD)
        LOG_WARN("packet is larger (%zu bytes) than MAX_PAYLOAD (%u bytes)", data_len, MAX_PAYLOAD);

    if ((ret = kvz_rtp::sender::write_rtp_header(conn, timestamp)) != RTP_OK) {
        LOG_ERROR("Failed to write RTP Header for Opus frame!");
        return ret;
    }

    return kvz_rtp::sender::write_payload(conn, data, data_len);
}

kvz_rtp::frame::rtp_frame *kvz_rtp::generic::process_generic_frame(
    kvz_rtp::frame::rtp_frame *frame,
    std::vector<kvz_rtp::frame::rtp_frame *>& fu,
    rtp_error_t& error
)
{
    (void)fu;

    if (!frame) {
        error = RTP_INVALID_VALUE;

        LOG_ERROR("Invalid value, unable to process frame!");
        return nullptr;
    }

    return nullptr;
}
