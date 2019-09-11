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
#include "formats/generic.hh"
#include "send.hh"
#include "util.hh"
#include "writer.hh"

rtp_error_t kvz_rtp::send::send_frame(
    kvz_rtp::connection *conn,
    uint8_t *frame, size_t frame_len
)
{
    if (!conn || !frame || frame_len == 0)
        return RTP_INVALID_VALUE;

    conn->inc_sent_bytes(frame_len);
    conn->inc_sent_pkts();
    conn->inc_rtp_sequence();

    return conn->get_socket().sendto(frame, frame_len, 0, NULL);
}

rtp_error_t kvz_rtp::send::send_frame(
    kvz_rtp::connection *conn,
    uint8_t *header,  size_t header_len,
    uint8_t *payload, size_t payload_len
)
{
    if (!conn || !header || header_len == 0 || !payload || payload_len == 0)
        return RTP_INVALID_VALUE;

    std::vector<std::pair<size_t, uint8_t *>> buffers;

    conn->inc_sent_bytes(payload_len);
    conn->inc_sent_pkts();
    conn->inc_rtp_sequence();

    buffers.push_back(std::make_pair(header_len,  header));
    buffers.push_back(std::make_pair(payload_len, payload));

    return conn->get_socket().sendto(buffers, 0);
}

rtp_error_t kvz_rtp::send::send_frame(
    kvz_rtp::connection *conn,
    std::vector<std::pair<size_t, uint8_t *>>& buffers
)
{
    if (!conn)
        return RTP_INVALID_VALUE;

    size_t total_size = 0;

    /* first buffer is supposed to be RTP header which is not included */
    for (size_t i = 1; i < buffers.size(); ++i) {
        total_size += buffers.at(i).first;
    }

    conn->inc_sent_bytes(total_size);
    conn->inc_sent_pkts();
    conn->inc_rtp_sequence();

    return conn->get_socket().sendto(buffers, 0);
}
