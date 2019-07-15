#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#else
#include <arpa/inet.h>
#include <sys/types.h>
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

rtp_error_t kvz_rtp::sender::write_payload(kvz_rtp::connection *conn, uint8_t *payload, size_t payload_len)
{
    if (!conn)
        return RTP_INVALID_VALUE;

    conn->inc_sent_bytes(payload_len);
    conn->inc_sent_pkts();
    conn->inc_rtp_sequence();

    return conn->get_socket().sendto(payload, payload_len, 0, NULL);
}

rtp_error_t kvz_rtp::sender::write_generic_header(kvz_rtp::connection *conn, uint8_t *header, size_t header_len)
{
    if (!conn)
        return RTP_INVALID_VALUE;

#ifdef __linux__
    return conn->get_socket().sendto(header, header_len, MSG_MORE, NULL);
#else
    return conn->get_socket().sendto(header, header_len, MSG_PARTIAL, NULL);
#endif
}

rtp_error_t kvz_rtp::sender::write_rtp_header(kvz_rtp::connection *conn, uint32_t timestamp)
{
    if (!conn)
        return RTP_INVALID_VALUE;

    uint8_t header[kvz_rtp::frame::HEADER_SIZE_RTP] = { 0 };
    conn->fill_rtp_header(header, timestamp);

    return kvz_rtp::sender::write_generic_header(conn, header, kvz_rtp::frame::HEADER_SIZE_RTP);
}

rtp_error_t kvz_rtp::sender::write_generic_frame(kvz_rtp::connection *conn, kvz_rtp::frame::rtp_frame *frame)
{
    if (!frame)
        return RTP_INVALID_VALUE;

    rtp_error_t ret;

    if ((ret = kvz_rtp::sender::write_payload(conn, frame->data, frame->total_len)) != RTP_OK) {
        LOG_ERROR("Failed to send payload! Size %zu, Type %d", frame->total_len, frame->type);
        return ret;
    }

    return RTP_OK;
}

rtp_error_t kvz_rtp::sender::write_frame(
    kvz_rtp::connection *conn,
    uint8_t *header,  size_t header_len,
    uint8_t *payload, size_t payload_len
)
{
    if (!conn || !header || !payload || header_len == 0 || payload_len == 0)
        return RTP_INVALID_VALUE;

    rtp_error_t ret;

    if ((ret = kvz_rtp::sender::write_generic_header(conn, header, header_len)) != RTP_OK) {
        LOG_ERROR("Failed to write generic header, length: %zu", header_len);
        return ret;
    }

    if ((ret = kvz_rtp::sender::write_payload(conn, payload, payload_len)) != RTP_OK) {
        LOG_ERROR("Failed to write payload, length: %zu", payload_len);
        return ret;
    }

    return ret;
}
