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
        LOG_WARN("packet is larger (%zu bytes) than MAX_PAYLOAD (%zu bytes)", data_len, MAX_PAYLOAD);

    if ((ret = kvz_rtp::sender::write_rtp_header(conn, timestamp)) != RTP_OK) {
        LOG_ERROR("Failed to write RTP Header for Opus frame!");
        return ret;
    }

    return kvz_rtp::sender::write_payload(conn, data, data_len);
}
