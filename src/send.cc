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

#ifdef __RTP_STATS__
    conn->incProcessedBytes(payload_len);
    conn->incTotalBytes(payload_len);
    conn->incProcessedPackets(1);
#endif

    conn->incRTPSequence(1);

    return conn->get_socket().sendto(payload, payload_len, 0, NULL);
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

#ifdef __linux__
    /* TODO: error checking */
    (void)kvz_rtp::sender::enqueue_message(conn, header, header_len, payload, payload_len);
    ret = kvz_rtp::sender::flush_message_queue(conn);
#else
    if ((ret = kvz_rtp::sender::write_generic_header(conn, header, header_len)) != RTP_OK) {
        LOG_ERROR("Failed to write generic header, length: %zu", header_len);
        return ret;
    }

    if ((ret = kvz_rtp::sender::write_payload(conn, payload, payload_len)) != RTP_OK) {
        LOG_ERROR("Failed to write payload, length: %zu", payload_len);
        return ret;
    }
#endif

    return ret;
}

#define MAX_CHUNK_COUNT 2000
#define MAX_MSG_COUNT   1000

static thread_local struct mmsghdr headers[MAX_MSG_COUNT];
static thread_local struct msghdr messages[MAX_MSG_COUNT];
static thread_local struct iovec chunks[MAX_CHUNK_COUNT];
static thread_local int chunk_ptr = 0;
static thread_local int hdr_ptr = 0;
static thread_local int msg_ptr = 0;

rtp_error_t kvz_rtp::sender::enqueue_message(
    kvz_rtp::connection *conn,
    uint8_t *header,  size_t header_len,
    uint8_t *payload, size_t payload_len
)
{
    if (!conn || !header || header_len == 0|| !payload || payload_len == 0)
        return RTP_INVALID_VALUE;

    if (chunk_ptr + 2 >= MAX_CHUNK_COUNT || msg_ptr + 1 >= MAX_MSG_COUNT) {
        LOG_ERROR("maximum amount of chunks (%d) or messages (%d) exceeded!", chunk_ptr, msg_ptr);
        return RTP_MEMORY_ERROR;
    }

    sockaddr_in out_addr = dynamic_cast<kvz_rtp::writer *>(conn)->get_out_address(); 

    chunks[chunk_ptr + 0].iov_base = header;
    chunks[chunk_ptr + 0].iov_len  = header_len;

    chunks[chunk_ptr + 1].iov_base = payload;
    chunks[chunk_ptr + 1].iov_len  = payload_len;

    messages[msg_ptr].msg_name = (void *)&out_addr;
    messages[msg_ptr].msg_namelen = sizeof(out_addr);
    messages[msg_ptr].msg_iov = &chunks[chunk_ptr];
    messages[msg_ptr].msg_iovlen = 2;
    messages[msg_ptr].msg_control = 0;
    messages[msg_ptr].msg_controllen = 0;

    headers[hdr_ptr].msg_hdr = messages[msg_ptr];

    chunk_ptr += 2;
    msg_ptr   += 1;
    hdr_ptr   += 1;

    conn->incRTPSequence(1);

    return RTP_OK;
}

rtp_error_t kvz_rtp::sender::enqueue_message(
    kvz_rtp::connection *conn,
    uint8_t *message, size_t message_len
)
{
    if (!conn || !message || message_len == 0)
        return RTP_INVALID_VALUE;

    if (chunk_ptr + 1 >= MAX_CHUNK_COUNT || msg_ptr + 1 >= MAX_MSG_COUNT) {
        LOG_ERROR("maximum amount of chunks (%d) or messages (%d) exceeded!", chunk_ptr, msg_ptr);
        return RTP_MEMORY_ERROR;
    }

    sockaddr_in out_addr = dynamic_cast<kvz_rtp::writer *>(conn)->get_out_address(); 

    chunks[chunk_ptr + 0].iov_base = message;
    chunks[chunk_ptr + 0].iov_len  = message_len;

    messages[msg_ptr].msg_name = (void *)&out_addr;
    messages[msg_ptr].msg_namelen = sizeof(out_addr);
    messages[msg_ptr].msg_iov = &chunks[chunk_ptr];
    messages[msg_ptr].msg_iovlen = 1;
    messages[msg_ptr].msg_control = 0;
    messages[msg_ptr].msg_controllen = 0;

    headers[hdr_ptr].msg_hdr = messages[msg_ptr];

    chunk_ptr++;
    msg_ptr++;
    hdr_ptr++;

    conn->incRTPSequence(1);

    return RTP_OK;
}

rtp_error_t kvz_rtp::sender::enqueue_message(
    kvz_rtp::connection *conn,
    std::vector<std::pair<size_t, uint8_t *>>& buffers
)
{
    if (!conn || buffers.size() == 0)
        return RTP_INVALID_VALUE;

    if (chunk_ptr + buffers.size() >= MAX_CHUNK_COUNT || msg_ptr + 1 >= MAX_MSG_COUNT) {
        LOG_ERROR("maximum amount of chunks (%d) or messages (%d) exceeded!", chunk_ptr, msg_ptr);
        return RTP_MEMORY_ERROR;
    }

    for (size_t i = 0; i < buffers.size(); ++i) {
        chunks[chunk_ptr + i].iov_len  = buffers.at(i).first;
        chunks[chunk_ptr + i].iov_base = buffers.at(i).second;
    }

    sockaddr_in out_addr = dynamic_cast<kvz_rtp::writer *>(conn)->get_out_address(); 

    messages[msg_ptr].msg_name = (void *)&out_addr;
    messages[msg_ptr].msg_namelen = sizeof(out_addr);
    messages[msg_ptr].msg_iov = &chunks[chunk_ptr];
    messages[msg_ptr].msg_iovlen = buffers.size();
    messages[msg_ptr].msg_control = 0;
    messages[msg_ptr].msg_controllen = 0;

    headers[hdr_ptr].msg_hdr = messages[msg_ptr];

    chunk_ptr += buffers.size();
    msg_ptr   += 1;
    hdr_ptr   += 1;

    conn->incRTPSequence(1);

    return RTP_OK;
}

rtp_error_t kvz_rtp::sender::flush_message_queue(kvz_rtp::connection *conn)
{
    rtp_error_t ret = RTP_OK;

    if (msg_ptr == 0 || hdr_ptr == 0 || chunk_ptr == 0) {
        LOG_ERROR("Cannot send 0 messages or messages containing 0 chunks!");
        ret = RTP_INVALID_VALUE;
        goto end;
    }

    if (sendmmsg(conn->get_raw_socket(), headers, hdr_ptr, 0) < 0) {
        LOG_ERROR("Failed to flush the message queue!");
        ret = RTP_SEND_ERROR;
    }

    LOG_DEBUG("full message took %d chunks and %d messages", chunk_ptr, msg_ptr);

end:
    chunk_ptr = hdr_ptr = msg_ptr = 0;
    return ret;
}
