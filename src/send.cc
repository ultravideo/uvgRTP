#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
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

static rtp_error_t __internal_write(kvz_rtp::connection *conn, uint8_t *buf, size_t buf_len, int flags)
{
    if (!buf || buf_len == 0)
        return RTP_INVALID_VALUE;

    kvz_rtp::writer *writer = dynamic_cast<kvz_rtp::writer *>(conn);

#ifdef __linux__
    sockaddr_in out_addr    = writer->get_out_address();

    if (sendto(conn->get_socket(), buf, buf_len, flags, (struct sockaddr *)&out_addr, sizeof(out_addr)) == -1)
        return RTP_SEND_ERROR;
#else
    DWORD sent_bytes;
    WSABUF data_buf;

    data_buf.buf = (char *)buf;
    data_buf.len = buf_len;

    if (WSASend((SOCKET)conn->get_socket(), &data_buf, 1, &sent_bytes, flags, NULL, NULL) == -1)
        return RTP_SEND_ERROR;
#endif

    return RTP_OK;
}

rtp_error_t kvz_rtp::sender::write_payload(kvz_rtp::connection *conn, uint8_t *payload, size_t payload_len)
{
    if (!conn)
        return RTP_INVALID_VALUE;

#ifdef __RTP_STATS__
    conn->incProcessedBytes(payload_len);
    conn->incTotalBytes(payload_len);
    conn->incProcessedPackets(1);
#endif

    conn->incRTPSequence(1);

    return __internal_write(conn, payload, payload_len, 0);
}

rtp_error_t kvz_rtp::sender::write_generic_header(kvz_rtp::connection *conn, uint8_t *header, size_t header_len)
{
    if (!conn)
        return RTP_INVALID_VALUE;

#ifdef __RTP_STATS__
    conn->incOverheadBytes(header_len);
    conn->incTotalBytes(header_len);
#endif

#ifdef __linux__
    return __internal_write(conn, header, header_len, MSG_MORE);
#else
    return __internal_write(conn, header, header_len, MSG_PARTIAL);
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
