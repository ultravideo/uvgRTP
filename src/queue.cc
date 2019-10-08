#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#else
#include <arpa/inet.h>
#include <sys/types.h>
#include <cassert>
#include <cstring>
#endif

#include "conn.hh"
#include "debug.hh"
#include "queue.hh"
#include "random.hh"
#include "writer.hh"

#include "formats/hevc.hh"

kvz_rtp::frame_queue::frame_queue(rtp_format_t fmt):
    fmt_(fmt), active_(nullptr)
{
}

kvz_rtp::frame_queue::~frame_queue()
{
}

rtp_error_t kvz_rtp::frame_queue::init_transaction(kvz_rtp::connection *conn)
{
    std::lock_guard<std::mutex> lock(transaction_mtx_);

    if (active_ != nullptr) {
        LOG_ERROR("trying to initialize a new transaction while previous is still active!");
        return RTP_GENERIC_ERROR;
    }

    if (free_.empty()) {
        active_      = new transaction_t;
        active_->key = kvz_rtp::random::generate_32();

        switch (fmt_) {
            case RTP_FORMAT_HEVC:
                active_->media_headers = new kvz_rtp::hevc::media_headers;
                break;
        }
    } else {
        active_ = free_.back();
        free_.pop_back();
    }

    active_->chunk_ptr  = 0;
    active_->hdr_ptr    = 0;
    active_->rtphdr_ptr = 0;

    active_->out_addr = dynamic_cast<kvz_rtp::writer *>(conn)->get_out_address();
    conn->fill_rtp_header((uint8_t *)&active_->rtp_common);

    active_->buffers.clear();

    return RTP_OK;
}

rtp_error_t kvz_rtp::frame_queue::deinit_transaction(uint32_t key)
{
    std::lock_guard<std::mutex> lock(transaction_mtx_);
    auto transaction_it = queued_.find(key);

    if (transaction_it == queued_.end())
        return RTP_INVALID_VALUE;

    if (active_ && active_->key == key)
        active_ = nullptr;

    queued_.erase(key);

    if (free_.size() > MAX_QUEUED_MSGS) {
        delete transaction_it->second->media_headers;
        delete transaction_it->second;
    } else {
        free_.push_back(transaction_it->second);
    }

    return RTP_OK;
}

rtp_error_t kvz_rtp::frame_queue::deinit_transaction()
{
    if (active_ == nullptr) {
        LOG_ERROR("Trying to deinit transaction, no active transaction!");
        return RTP_INVALID_VALUE;
    }

    return kvz_rtp::frame_queue::deinit_transaction(active_->key);
}

rtp_error_t kvz_rtp::frame_queue::enqueue_message(
    kvz_rtp::connection *conn,
    uint8_t *message, size_t message_len
)
{
    if (!conn || !message || message_len == 0)
        return RTP_INVALID_VALUE;

#ifdef __linux__
    if (active_->chunk_ptr + 2 >= MAX_CHUNK_COUNT || active_->hdr_ptr + 1 >= MAX_MSG_COUNT) {
        LOG_ERROR("maximum amount of chunks (%d) or messages (%d) exceeded!", active_->chunk_ptr, active_->hdr_ptr);
        return RTP_MEMORY_ERROR;
    }

    /* update the RTP header at "rtpheaders_ptr_" */
    kvz_rtp::frame_queue::update_rtp_header(conn);

    active_->chunks[active_->chunk_ptr + 0].iov_base = &active_->rtp_headers[active_->rtphdr_ptr];
    active_->chunks[active_->chunk_ptr + 0].iov_len  = sizeof(active_->rtp_headers[active_->rtphdr_ptr]);

    active_->chunks[active_->chunk_ptr + 1].iov_base = message;
    active_->chunks[active_->chunk_ptr + 1].iov_len  = message_len;

    active_->headers[active_->hdr_ptr].msg_hdr.msg_name       = (void *)&active_->out_addr;
    active_->headers[active_->hdr_ptr].msg_hdr.msg_namelen    = sizeof(active_->out_addr);
    active_->headers[active_->hdr_ptr].msg_hdr.msg_iov        = &active_->chunks[active_->chunk_ptr];
    active_->headers[active_->hdr_ptr].msg_hdr.msg_iovlen     = 2;
    active_->headers[active_->hdr_ptr].msg_hdr.msg_control    = 0;
    active_->headers[active_->hdr_ptr].msg_hdr.msg_controllen = 0;

    active_->rtphdr_ptr += 1;
    active_->chunk_ptr  += 2;
    active_->hdr_ptr    += 1;
#else
    /* TODO: winsock stuff */
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
    if (!conn || buffers.size() == 0)
        return RTP_INVALID_VALUE;

#ifdef __linux__
    if (active_->chunk_ptr + buffers.size() + 1 >= MAX_CHUNK_COUNT || active_->hdr_ptr + 1 >= MAX_MSG_COUNT) {
        LOG_ERROR("maximum amount of chunks (%d) or messages (%d) exceeded!", active_->chunk_ptr, active_->hdr_ptr);
        return RTP_MEMORY_ERROR;
    }

    /* update the RTP header at "rtpheaders_ptr_" */
    kvz_rtp::frame_queue::update_rtp_header(conn);

    active_->chunks[active_->chunk_ptr].iov_base = &active_->rtp_headers[active_->rtphdr_ptr];
    active_->chunks[active_->chunk_ptr].iov_len  = sizeof(active_->rtp_headers[active_->rtphdr_ptr]);

    for (size_t i = 0; i < buffers.size(); ++i) {
        active_->chunks[active_->chunk_ptr + i + 1].iov_len  = buffers.at(i).first;
        active_->chunks[active_->chunk_ptr + i + 1].iov_base = buffers.at(i).second;
    }

    active_->headers[active_->hdr_ptr].msg_hdr.msg_name       = (void *)&active_->out_addr;
    active_->headers[active_->hdr_ptr].msg_hdr.msg_namelen    = sizeof(active_->out_addr);
    active_->headers[active_->hdr_ptr].msg_hdr.msg_iov        = &active_->chunks[active_->chunk_ptr];
    active_->headers[active_->hdr_ptr].msg_hdr.msg_iovlen     = buffers.size() + 1;
    active_->headers[active_->hdr_ptr].msg_hdr.msg_control    = 0;
    active_->headers[active_->hdr_ptr].msg_hdr.msg_controllen = 0;

    active_->chunk_ptr  += buffers.size() + 1;
    active_->hdr_ptr    += 1;
    active_->rtphdr_ptr += 1;
#else
    /* TODO: winsock stuff */
#endif

    conn->inc_rtp_sequence();
    conn->inc_sent_pkts();

    return RTP_OK;
}

rtp_error_t kvz_rtp::frame_queue::flush_queue(kvz_rtp::connection *conn)
{
    /* set the marker bit of the last packet to 1 */
    ((uint8_t *)&active_->rtp_headers[active_->rtphdr_ptr - 1])[1] |= (1 << 7);

#ifdef __linux__
    transaction_mtx_.lock();
    queued_.insert(std::make_pair(active_->key, active_));
    transaction_mtx_.unlock();

#ifdef __RTP_USE_SYSCALL_DISPATCHER__
    h_sfptr_      = active_->hdr_ptr;
    c_sfptr_      = active_->chunk_ptr;

    active_.h_end = active_->hdr_ptr   - 1;
    active_.c_end = active_->chunk_ptr - 1;

    dispatcher_->trigger_send(this);
#else
    if (!conn || active_->hdr_ptr == 0 || active_->chunk_ptr == 0) {
        LOG_ERROR("Cannot send 0 messages or messages containing 0 chunks!");
        (void)deinit_transaction();
        return RTP_INVALID_VALUE;
    }

    if (conn->get_socket().send_vecio(active_->headers, active_->hdr_ptr, 0) != RTP_OK) {
        LOG_ERROR("Failed to flush the message queue: %s", strerror(errno));
        (void)deinit_transaction();
        return RTP_SEND_ERROR;
    }

    LOG_DEBUG("full message took %d chunks and %d messages", active_->chunk_ptr, active_->hdr_ptr);

    return deinit_transaction();
#endif
#endif
}

void kvz_rtp::frame_queue::update_rtp_header(kvz_rtp::connection *conn)
{
    memcpy(&active_->rtp_headers[active_->rtphdr_ptr], &active_->rtp_common, sizeof(active_->rtp_common));
    conn->update_rtp_sequence((uint8_t *)(&active_->rtp_headers[active_->rtphdr_ptr]));
}

kvz_rtp::buff_vec& kvz_rtp::frame_queue::get_buffer_vector()
{
    return active_->buffers;
}

void *kvz_rtp::frame_queue::get_media_headers()
{
    return active_->media_headers;
}
