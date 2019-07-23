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
#include <iostream>

kvz_rtp::frame_queue::frame_queue()
#ifdef __linux__
    :hdr_ptr_(0), msg_ptr_(0), chunk_ptr_(0)
#else
    :buf_ptr_(0)
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

    /* TODO: this is not valid after return */
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
#else
    (void)conn;

    if  (!header || header_len == 0 || !payload || payload_len == 0)
        return RTP_INVALID_VALUE;

    if (buf_ptr_ + 1 >= MAX_CHUNK_COUNT)
        return RTP_MEMORY_ERROR;

    uint8_t *tmp = new uint8_t[header_len + payload_len];

    memcpy(tmp,              header,  header_len);
    memcpy(tmp + header_len, payload, payload_len);

    buffers_[buf_ptr_].buf = (char *)tmp;
    buffers_[buf_ptr_].len = (u_long)(header_len + payload_len);

    merge_bufs_.push_back(tmp);
    buf_ptr_++;
#endif

    conn->inc_rtp_sequence();
    conn->inc_sent_pkts();

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

    /* TODO: this is not valid after return */
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
#else
    (void)conn;

    if (!message || !message_len == 0)
        return RTP_INVALID_VALUE;

    if (buf_ptr_ + 1 >= MAX_CHUNK_COUNT)
        return RTP_MEMORY_ERROR;

    buffers_[buf_ptr_].len = (u_long)message_len;
    buffers_[buf_ptr_].buf = (char *)message;

    buf_ptr_++;
#endif

    conn->inc_rtp_sequence();
    conn->inc_sent_pkts();

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

    /* TODO: this is not valid after return */
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
#else
    (void)conn;
    DWORD sent_bytes;

    if (buf_ptr_ + buffers.size() >= MAX_CHUNK_COUNT)
        return RTP_MEMORY_ERROR;

    unsigned ptr = 0, total_size = 0;
    uint8_t *tmp = nullptr;

    for (size_t i = 0; i < buffers.size(); ++i) {
        total_size += buffers.at(i).first;
    }

    tmp = new uint8_t[total_size];

    for (size_t i = 0; i < buffers.size(); ++i) {
        mempcpy(tmp + ptr, buffers.at(i).second, buffers.at(i).first);
        ptr += buffers.at(i).first;
    }

    buffers_[buf_ptr_].buf = (char *)tmp;
    buffers_[buf_ptr_].len = (u_long)total_size;

    merge_bufs_.push_back(tmp);
    buf_ptr_++;
#endif

    conn->inc_rtp_sequence();
    conn->inc_sent_pkts();

    return RTP_OK;
}

rtp_error_t kvz_rtp::frame_queue::flush_queue(kvz_rtp::connection *conn)
{
#ifdef __linux__

    if (!conn || msg_ptr_ == 0 || hdr_ptr_ == 0 || chunk_ptr_ == 0) {
        LOG_ERROR("Cannot send 0 messages or messages containing 0 chunks!");
        empty_queue();
        return RTP_INVALID_VALUE;
    }

    if (sendmmsg(conn->get_raw_socket(), headers_, hdr_ptr_, 0) < 0) {
        LOG_ERROR("Failed to flush the message queue!");
        empty_queue();
        return RTP_SEND_ERROR;
    }

    LOG_DEBUG("full message took %d chunks and %d messages", chunk_ptr_, msg_ptr_);

    return kvz_rtp::frame_queue::empty_queue();
#else
    if (!conn || buf_ptr_ == 0)
        return RTP_INVALID_VALUE;

    sockaddr_in addr = dynamic_cast<kvz_rtp::writer *>(conn)->get_out_address();
    DWORD sent_bytes;

    if (WSASendTo(conn->get_raw_socket(), buffers_, buf_ptr_, &sent_bytes, 0, (SOCKADDR *)&addr, sizeof(addr), NULL, NULL) == -1) {
        win_get_last_error();
        empty_queue();
        return RTP_SEND_ERROR;
    }

    std::cerr << std::endl << "FLUSHING QUEUE!" << std::endl;
    LOG_INFO("full message took %d buffers, sent %u bytes", buf_ptr_, sent_bytes);
    return empty_queue();
#endif
}

rtp_error_t kvz_rtp::frame_queue::empty_queue()
{
#ifdef __linux__
    hdr_ptr_ = msg_ptr_ = chunk_ptr_ = 0;
#else
    buf_ptr_ = 0;

    for (size_t i = 0; i < merge_bufs_.size(); ++i) {
        auto buffer = merge_bufs_.back();
        merge_bufs_.pop_back();
        delete[] buffer;
    }
#endif

    return RTP_OK;
}
