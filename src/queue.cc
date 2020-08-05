#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#else
#include <arpa/inet.h>
#include <sys/types.h>
#include <cassert>
#include <cstring>
#endif

#include "debug.hh"
#include "queue.hh"
#include "random.hh"

#include "formats/hevc.hh"

/* uvg_rtp::frame_queue::frame_queue(rtp_format_t fmt): */
/*     active_(nullptr), fmt_(fmt), dealloc_hook_(nullptr) */
/* { */
/*     active_     = nullptr; */
/*     dispatcher_ = nullptr; */

/*     max_queued_ = MAX_QUEUED_MSGS; */
/*     max_mcount_ = MAX_MSG_COUNT; */
/*     max_ccount_ = MAX_CHUNK_COUNT * max_mcount_; */
/* } */

/* uvg_rtp::frame_queue::frame_queue(rtp_format_t fmt, uvg_rtp::dispatcher *dispatcher): */
/*     frame_queue(fmt) */
/* { */
/*     dispatcher_ = dispatcher; */
/* } */

uvg_rtp::frame_queue::frame_queue(uvg_rtp::socket *socket, uvg_rtp::rtp *rtp):
    rtp_(rtp), socket_(socket)
{
    active_     = nullptr;
    dispatcher_ = nullptr;

    max_queued_ = MAX_QUEUED_MSGS;
    max_mcount_ = MAX_MSG_COUNT;
    max_ccount_ = MAX_CHUNK_COUNT * max_mcount_;
}

uvg_rtp::frame_queue::~frame_queue()
{
    for (auto& i : free_) {
        (void)destroy_transaction(i);
    }
    free_.clear();

    if (active_)
        (void)destroy_transaction(active_);
}

rtp_error_t uvg_rtp::frame_queue::init_transaction()
{
    std::lock_guard<std::mutex> lock(transaction_mtx_);

    if (active_ != nullptr)
        active_ = nullptr;

    if (free_.empty()) {
        active_      = new transaction_t;
        active_->key = uvg_rtp::random::generate_32();

#ifdef __linux__
        active_->headers     = new struct mmsghdr[max_mcount_];
        active_->chunks      = new struct iovec[max_ccount_];
#else
        active_->headers     = nullptr;
        active_->chunks      = nullptr;
#endif
        active_->rtp_headers = new uvg_rtp::frame::rtp_header[max_mcount_];

        switch (rtp_->get_payload()) {
            case RTP_FORMAT_HEVC:
                active_->media_headers = new uvg_rtp::formats::hevc_headers;
                break;

            default:
                break;
        }
    } else {
        active_ = free_.back();
        free_.pop_back();
    }

    active_->chunk_ptr  = 0;
    active_->hdr_ptr    = 0;
    active_->rtphdr_ptr = 0;
    active_->fqueue     = this;

    active_->data_raw     = nullptr;
    active_->data_smart   = nullptr;
    active_->dealloc_hook = dealloc_hook_;

    active_->out_addr = socket_->get_out_address();
    rtp_->fill_header((uint8_t *)&active_->rtp_common);
    active_->buffers.clear();

    return RTP_OK;
}

rtp_error_t uvg_rtp::frame_queue::init_transaction(uint8_t *data)
{
    if (!data)
        return RTP_INVALID_VALUE;

    if (init_transaction() != RTP_OK) {
        LOG_ERROR("Failed to initialize transaction");
        return RTP_GENERIC_ERROR;
    }

    /* The transaction has been initialized to "active_" */
    active_->data_raw = data;

    return RTP_OK;
}

rtp_error_t uvg_rtp::frame_queue::init_transaction(std::unique_ptr<uint8_t[]> data)
{
    if (!data)
        return RTP_INVALID_VALUE;

    if (init_transaction() != RTP_OK) {
        LOG_ERROR("Failed to initialize transaction");
        return RTP_GENERIC_ERROR;
    }

    /* The transaction has been initialized to "active_" */
    active_->data_smart = std::move(data);

    return RTP_OK;
}

rtp_error_t uvg_rtp::frame_queue::destroy_transaction(uvg_rtp::transaction_t *t)
{
    if (!t)
        return RTP_INVALID_VALUE;

    delete[] t->headers;
    delete[] t->chunks;
    delete[] t->rtp_headers;

    t->headers     = nullptr;
    t->chunks      = nullptr;
    t->rtp_headers = nullptr;

    switch (rtp_->get_payload()) {
        case RTP_FORMAT_HEVC:
            delete (uvg_rtp::formats::hevc_headers *)t->media_headers;
            t->media_headers = nullptr;
            break;

        default:
            break;
    }
    delete t;
    t = nullptr;

    return RTP_OK;
}

rtp_error_t uvg_rtp::frame_queue::deinit_transaction(uint32_t key)
{
    std::lock_guard<std::mutex> lock(transaction_mtx_);

    auto transaction_it = queued_.find(key);

    if (transaction_it == queued_.end()) {
        /* It's possible that the transaction has not been queued yet because
         * the chunk given by the application was smaller than MTU */
        if (active_ && active_->key == key) {
            free_.push_back(active_);
            active_ = nullptr;
            return RTP_OK;
        }

        return RTP_INVALID_VALUE;
    }

    if (active_ && active_->key == key) {
        free_.push_back(active_);
        active_ = nullptr;
        return RTP_OK;
    }

    /* Deallocate the raw data pointer using the deallocation hook provided by application */
    if (transaction_it->second->data_raw && transaction_it->second->dealloc_hook) {
        transaction_it->second->dealloc_hook(transaction_it->second->data_raw);
        transaction_it->second->data_raw = nullptr;
    }

    if (free_.size() >= (size_t)max_queued_) {
        switch (rtp_->get_payload()) {
            case RTP_FORMAT_HEVC:
                delete (uvg_rtp::formats::hevc_headers *)transaction_it->second->media_headers;
                break;

            default:
                break;
        }

        delete[] transaction_it->second->headers;
        delete[] transaction_it->second->chunks;
        delete[] transaction_it->second->rtp_headers;
        delete   transaction_it->second;
    } else {
        free_.push_back(transaction_it->second);
    }

    queued_.erase(key);
    return RTP_OK;
}

rtp_error_t uvg_rtp::frame_queue::deinit_transaction()
{
    if (active_ == nullptr) {
        LOG_WARN("Trying to deinit transaction, no active transaction!");
        return RTP_INVALID_VALUE;
    }

    return uvg_rtp::frame_queue::deinit_transaction(active_->key);
}

rtp_error_t uvg_rtp::frame_queue::enqueue_message(uint8_t *message, size_t message_len)
{
    if (!message || !message_len)
        return RTP_INVALID_VALUE;

#ifdef __linux__
    if (active_->chunk_ptr + 2 >= (size_t)max_ccount_ || active_->hdr_ptr + 1 >= (size_t)max_mcount_) {
        LOG_ERROR("maximum amount of chunks (%zu) or messages (%zu) exceeded!", active_->chunk_ptr, active_->hdr_ptr);
        return RTP_MEMORY_ERROR;
    }

    /* update the RTP header at "rtpheaders_ptr_" */
    uvg_rtp::frame_queue::update_rtp_header();

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

    rtp_->inc_sequence();
    rtp_->inc_sent_pkts();

    return RTP_OK;
}

rtp_error_t uvg_rtp::frame_queue::enqueue_message(std::vector<std::pair<size_t, uint8_t *>>& buffers)
{
    if (!buffers.size())
        return RTP_INVALID_VALUE;

#ifdef __linux__
    if (active_->chunk_ptr + buffers.size() + 1 >= (size_t)max_ccount_ || active_->hdr_ptr + 1 >= (size_t)max_mcount_) {
        LOG_ERROR("maximum amount of chunks (%zu) or messages (%zu) exceeded!", active_->chunk_ptr, active_->hdr_ptr);
        return RTP_MEMORY_ERROR;
    }

    /* update the RTP header at "rtpheaders_ptr_" */
    uvg_rtp::frame_queue::update_rtp_header();

    active_->chunks[active_->chunk_ptr].iov_len  = sizeof(active_->rtp_headers[active_->rtphdr_ptr]);
    active_->chunks[active_->chunk_ptr].iov_base = &active_->rtp_headers[active_->rtphdr_ptr];

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

    rtp_->inc_sequence();
    rtp_->inc_sent_pkts();

    return RTP_OK;
}

rtp_error_t uvg_rtp::frame_queue::flush_queue()
{
    if (!active_->hdr_ptr || !active_->chunk_ptr) {
        LOG_ERROR("Cannot send 0 messages or messages containing 0 chunks!");
        (void)deinit_transaction();
        return RTP_INVALID_VALUE;
    }

    /* set the marker bit of the last packet to 1 */
    ((uint8_t *)&active_->rtp_headers[active_->rtphdr_ptr - 1])[1] |= (1 << 7);

#ifdef __linux__
    transaction_mtx_.lock();
    queued_.insert(std::make_pair(active_->key, active_));
    transaction_mtx_.unlock();

    if (dispatcher_) {
        dispatcher_->trigger_send(active_);
        active_ = nullptr;
        return RTP_OK;
    }

    if (socket_->send_vecio(active_->headers, active_->hdr_ptr, 0) != RTP_OK) {
        LOG_ERROR("Failed to flush the message queue: %s", strerror(errno));
        (void)deinit_transaction();
        return RTP_SEND_ERROR;
    }

    LOG_DEBUG("full message took %zu chunks and %zu messages", active_->chunk_ptr, active_->hdr_ptr);

    return deinit_transaction();
#endif
}

void uvg_rtp::frame_queue::update_rtp_header()
{
    memcpy(&active_->rtp_headers[active_->rtphdr_ptr], &active_->rtp_common, sizeof(active_->rtp_common));
    rtp_->update_sequence((uint8_t *)(&active_->rtp_headers[active_->rtphdr_ptr]));
}

uvg_rtp::buff_vec& uvg_rtp::frame_queue::get_buffer_vector()
{
    return active_->buffers;
}

void *uvg_rtp::frame_queue::get_media_headers()
{
    return active_->media_headers;
}

uint8_t *uvg_rtp::frame_queue::get_active_dataptr()
{
    if (!active_)
        return nullptr;

    if (active_->data_smart)
        return active_->data_smart.get();
    return active_->data_raw;
}

void uvg_rtp::frame_queue::install_dealloc_hook(void (*dealloc_hook)(void *))
{
    if (!dealloc_hook)
        return;

    dealloc_hook_ = dealloc_hook;
}
