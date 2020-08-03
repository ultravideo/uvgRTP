#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#else
#include <arpa/inet.h>
#include <sys/types.h>
#endif

#include <cstdint>
#include <cstring>
#include <iostream>

/* #include "debug.hh" */
/* #include "formats/generic.hh" */
#include "send.hh"
/* #include "sender.hh" */
/* #include "util.hh" */
/* #include "sender.hh" */

#if 0
rtp_error_t uvg_rtp::send::send_frame(
    uvg_rtp::sender *sender,
    uint8_t *frame, size_t frame_len
)
{
    if (!sender || !frame || frame_len == 0)
        return RTP_INVALID_VALUE;

    sender->get_rtp_ctx()->inc_sent_pkts();
    sender->get_rtp_ctx()->inc_sequence();

    return sender->get_socket().sendto(frame, frame_len, 0, NULL);
}

rtp_error_t uvg_rtp::send::send_frame(
    uvg_rtp::sender *sender,
    uint8_t *header,  size_t header_len,
    uint8_t *payload, size_t payload_len
)
{
    if (!sender || !header || header_len == 0 || !payload || payload_len == 0)
        return RTP_INVALID_VALUE;

    std::vector<std::pair<size_t, uint8_t *>> buffers;

    sender->get_rtp_ctx()->inc_sent_pkts();
    sender->get_rtp_ctx()->inc_sequence();

    buffers.push_back(std::make_pair(header_len,  header));
    buffers.push_back(std::make_pair(payload_len, payload));

    return sender->get_socket().sendto(buffers, 0);
}

rtp_error_t uvg_rtp::send::send_frame(
    uvg_rtp::sender *sender,
    std::vector<std::pair<size_t, uint8_t *>>& buffers
)
{
    if (!sender)
        return RTP_INVALID_VALUE;

    size_t total_size = 0;

    /* first buffer is supposed to be RTP header which is not included */
    for (size_t i = 1; i < buffers.size(); ++i) {
        total_size += buffers.at(i).first;
    }

    sender->get_rtp_ctx()->inc_sent_pkts();
    sender->get_rtp_ctx()->inc_sequence();

    return sender->get_socket().sendto(buffers, 0);
}
#endif
