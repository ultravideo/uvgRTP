#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#else
#include <arpa/inet.h>
#include <sys/types.h>
#endif

#include "debug.hh"
#include "queue.hh"
#include "writer.hh"

kvz_rtp::frame_queue::frame_queue()
#ifdef __linux__
    :hdr_ptr_(0), msg_ptr_(0), chunk_ptr_(0)
#endif
{
}

kvz_rtp::frame_queue::~frame_queue()
{
}

rtp_error_t kvz_rtp::frame_queue::enqueue_message(
    kvz_rtp::connection *conn,
    uint8_t *header,  size_t header_len,
    uint8_t *payload, size_t payload_len
)
{
#ifdef __linux__
    if (!conn || !header || header_len == 0|| !payload || payload_len == 0)
        return RTP_INVALID_VALUE;

    if (chunk_ptr_ + 2 >= MAX_CHUNK_COUNT || msg_ptr_ + 1 >= MAX_MSG_COUNT) {
        LOG_ERROR("maximum amount of chunks (%d) or messages (%d) exceeded!", chunk_ptr_, msg_ptr_);
        return RTP_MEMORY_ERROR;
    }

    sockaddr_in out_addr = dynamic_cast<kvz_rtp::writer *>(conn)->get_out_address(); 

    chunks_[chunk_ptr_ + 0].iov_base = header;
    chunks_[chunk_ptr_ + 0].iov_len  = header_len;

    chunks_[chunk_ptr_ + 1].iov_base = payload;
    chunks_[chunk_ptr_ + 1].iov_len  = payload_len;

    messages_[msg_ptr_].msg_name = (void *)&out_addr;
    messages_[msg_ptr_].msg_namelen = sizeof(out_addr);
    messages_[msg_ptr_].msg_iov = &chunks_[chunk_ptr_];
    messages_[msg_ptr_].msg_iovlen = 2;
    messages_[msg_ptr_].msg_control = 0;
    messages_[msg_ptr_].msg_controllen = 0;

    headers_[hdr_ptr_].msg_hdr = messages_[msg_ptr_];

    chunk_ptr_ += 2;
    msg_ptr_   += 1;
    hdr_ptr_   += 1;

    conn->inc_rtp_sequence();
    conn->inc_sent_pkts();
#else
    /* TODO: winsock */

    (void)conn, (void)header,(void)header_len, (void)payload, (void)payload_len;
#endif

    return RTP_OK;
}

rtp_error_t kvz_rtp::frame_queue::enqueue_message(
    kvz_rtp::connection *conn,
    uint8_t *message, size_t message_len
)
{
#ifdef __linux__
    if (!conn || !message || message_len == 0)
        return RTP_INVALID_VALUE;

    if (chunk_ptr_ + 1 >= MAX_CHUNK_COUNT || msg_ptr_ + 1 >= MAX_MSG_COUNT) {
        LOG_ERROR("maximum amount of chunks (%d) or messages (%d) exceeded!", chunk_ptr_, msg_ptr_);
        return RTP_MEMORY_ERROR;
    }

    sockaddr_in out_addr = dynamic_cast<kvz_rtp::writer *>(conn)->get_out_address(); 

    chunks_[chunk_ptr_ + 0].iov_base = message;
    chunks_[chunk_ptr_ + 0].iov_len  = message_len;

    messages_[msg_ptr_].msg_name = (void *)&out_addr;
    messages_[msg_ptr_].msg_namelen = sizeof(out_addr);
    messages_[msg_ptr_].msg_iov = &chunks_[chunk_ptr_];
    messages_[msg_ptr_].msg_iovlen = 1;
    messages_[msg_ptr_].msg_control = 0;
    messages_[msg_ptr_].msg_controllen = 0;

    headers_[hdr_ptr_].msg_hdr = messages_[msg_ptr_];

    chunk_ptr_++;
    msg_ptr_++;
    hdr_ptr_++;

    conn->inc_rtp_sequence();
    conn->inc_sent_pkts();
#else
    /* TODO: winsock */

    (void)conn, (void)message, (void)message_len;
#endif

    return RTP_OK;
}

rtp_error_t kvz_rtp::frame_queue::enqueue_message(
    kvz_rtp::connection *conn,
    std::vector<std::pair<size_t, uint8_t *>>& buffers
)
{
#ifdef __linux__
    if (!conn || buffers.size() == 0)
        return RTP_INVALID_VALUE;

    if (chunk_ptr_ + buffers.size() >= MAX_CHUNK_COUNT || msg_ptr_ + 1 >= MAX_MSG_COUNT) {
        LOG_ERROR("maximum amount of chunks (%d) or messages (%d) exceeded!", chunk_ptr_, msg_ptr_);
        return RTP_MEMORY_ERROR;
    }

    for (size_t i = 0; i < buffers.size(); ++i) {
        chunks_[chunk_ptr_ + i].iov_len  = buffers.at(i).first;
        chunks_[chunk_ptr_ + i].iov_base = buffers.at(i).second;
    }

    sockaddr_in out_addr = dynamic_cast<kvz_rtp::writer *>(conn)->get_out_address(); 

    messages_[msg_ptr_].msg_name = (void *)&out_addr;
    messages_[msg_ptr_].msg_namelen = sizeof(out_addr);
    messages_[msg_ptr_].msg_iov = &chunks_[chunk_ptr_];
    messages_[msg_ptr_].msg_iovlen = buffers.size();
    messages_[msg_ptr_].msg_control = 0;
    messages_[msg_ptr_].msg_controllen = 0;

    headers_[hdr_ptr_].msg_hdr = messages_[msg_ptr_];

    chunk_ptr_ += buffers.size();
    msg_ptr_   += 1;
    hdr_ptr_   += 1;

    conn->inc_rtp_sequence();
    conn->inc_sent_pkts();
#else
    /* TODO: winsock */

    (void)conn, (void)buffers;
#endif

    return RTP_OK;
}

rtp_error_t kvz_rtp::frame_queue::flush_queue(kvz_rtp::connection *conn)
{
#ifdef __linux__
    rtp_error_t ret = RTP_OK;

    if (msg_ptr_ == 0 || hdr_ptr_ == 0 || chunk_ptr_ == 0) {
        LOG_ERROR("Cannot send 0 messages or messages containing 0 chunks!");
        ret = RTP_INVALID_VALUE;
        goto end;
    }

    if (sendmmsg(conn->get_raw_socket(), headers_, hdr_ptr_, 0) < 0) {
        LOG_ERROR("Failed to flush the message queue!");
        ret = RTP_SEND_ERROR;
    }

    LOG_DEBUG("full message took %d chunks and %d messages", chunk_ptr_, msg_ptr_);

end:
    return kvz_rtp::frame_queue::empty_queue();
#else
    /* TODO: winsock */

    (void)conn;
#endif

    return RTP_OK;
}

rtp_error_t kvz_rtp::frame_queue::empty_queue()
{
#ifdef __linux__
    hdr_ptr_ = msg_ptr_ = chunk_ptr_ = 0;
#endif

    return RTP_OK;
}
