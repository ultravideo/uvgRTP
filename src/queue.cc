#include "queue.hh"

#include "formats/h264.hh"
#include "formats/h265.hh"
#include "formats/h266.hh"

#include "rtp.hh"
#include "srtp/base.hh"
#include "debug.hh"
#include "random.hh"

#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#else
#include <arpa/inet.h>
#include <sys/types.h>
#include <cassert>
#include <cstring>
#endif


uvgrtp::frame_queue::frame_queue(uvgrtp::socket *socket, uvgrtp::rtp *rtp, int flags):
    rtp_(rtp), socket_(socket), flags_(flags)
{
    active_     = nullptr;
    dispatcher_ = nullptr;

    max_queued_ = MAX_QUEUED_MSGS;
    max_mcount_ = MAX_MSG_COUNT;
    max_ccount_ = MAX_CHUNK_COUNT * max_mcount_;
}

uvgrtp::frame_queue::~frame_queue()
{
    for (auto& i : free_) {
        (void)destroy_transaction(i);
    }
    free_.clear();

    if (active_)
        (void)destroy_transaction(active_);
}

rtp_error_t uvgrtp::frame_queue::init_transaction()
{
    std::lock_guard<std::mutex> lock(transaction_mtx_);

    if (active_ != nullptr)
        active_ = nullptr;

    if (free_.empty()) {
        active_      = new transaction_t;
        active_->key = uvgrtp::random::generate_32();

#ifdef __linux__
        active_->headers     = new struct mmsghdr[max_mcount_];
        active_->chunks      = new struct iovec[max_ccount_];
#else
        active_->headers     = nullptr;
        active_->chunks      = nullptr;
#endif
        active_->rtp_headers = new uvgrtp::frame::rtp_header[max_mcount_];

        switch (rtp_->get_payload()) {
            case RTP_FORMAT_H264:
                active_->media_headers = new uvgrtp::formats::h264_headers;
                break;

            case RTP_FORMAT_H265:
                active_->media_headers = new uvgrtp::formats::h265_headers;
                break;

            case RTP_FORMAT_H266:
                active_->media_headers = new uvgrtp::formats::h266_headers;
                break;


            default:
                break;
        }
    } else {
        active_ = free_.back();
        free_.pop_back();
    }

    active_->chunk_ptr   = 0;
    active_->hdr_ptr     = 0;
    active_->rtphdr_ptr  = 0;
    active_->rtpauth_ptr = 0;
    active_->fqueue      = this;

    active_->data_raw     = nullptr;
    active_->data_smart   = nullptr;
    active_->dealloc_hook = dealloc_hook_;

    if (flags_ & RCE_SRTP_AUTHENTICATE_RTP)
        active_->rtp_auth_tags = new uint8_t[10 * max_mcount_];
    else
        active_->rtp_auth_tags = nullptr;

    active_->out_addr = socket_->get_out_address();
    rtp_->fill_header((uint8_t *)&active_->rtp_common);
    active_->buffers.clear();

    return RTP_OK;
}

rtp_error_t uvgrtp::frame_queue::init_transaction(uint8_t *data)
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

rtp_error_t uvgrtp::frame_queue::init_transaction(std::unique_ptr<uint8_t[]> data)
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

rtp_error_t uvgrtp::frame_queue::destroy_transaction(uvgrtp::transaction_t *t)
{
    if (!t)
        return RTP_INVALID_VALUE;

    delete[] t->headers;
    delete[] t->chunks;
    delete[] t->rtp_headers;
    delete[] t->rtp_auth_tags;

    t->headers     = nullptr;
    t->chunks      = nullptr;
    t->rtp_headers = nullptr;

    switch (rtp_->get_payload()) {
        case RTP_FORMAT_H264:
            delete (uvgrtp::formats::h264_headers *)t->media_headers;
            t->media_headers = nullptr;
            break;

        case RTP_FORMAT_H265:
            delete (uvgrtp::formats::h265_headers *)t->media_headers;
            t->media_headers = nullptr;
            break;

        case RTP_FORMAT_H266:
            delete (uvgrtp::formats::h266_headers *)t->media_headers;
            t->media_headers = nullptr;
            break;

        default:
            break;
    }
    delete t;
    t = nullptr;

    return RTP_OK;
}

rtp_error_t uvgrtp::frame_queue::deinit_transaction(uint32_t key)
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
        /* free all temporary buffers */
        if ((flags_ & (RCE_SRTP | RCE_SRTP_INPLACE_ENCRYPTION | RCE_SRTP_NULL_CIPHER)) == RCE_SRTP) {
            for (auto& packet : active_->packets) {
                for (size_t i = 1; i < packet.size(); ++i) {
                    delete[] packet[i].second;
                }
            }
        }
        active_->packets.clear();
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
            case RTP_FORMAT_H264:
                delete (uvgrtp::formats::h264_headers *)transaction_it->second->media_headers;
                transaction_it->second->media_headers = nullptr;
                break;

            case RTP_FORMAT_H265:
                delete (uvgrtp::formats::h265_headers *)transaction_it->second->media_headers;
                transaction_it->second->media_headers = nullptr;
                break;

            case RTP_FORMAT_H266:
                delete (uvgrtp::formats::h266_headers *)transaction_it->second->media_headers;
                transaction_it->second->media_headers = nullptr;
                break;

            default:
                break;
        }

        delete[] transaction_it->second->rtp_auth_tags;
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

rtp_error_t uvgrtp::frame_queue::deinit_transaction()
{
    if (active_ == nullptr) {
        LOG_WARN("Trying to deinit transaction, no active transaction!");
        return RTP_INVALID_VALUE;
    }

    return uvgrtp::frame_queue::deinit_transaction(active_->key);
}

rtp_error_t uvgrtp::frame_queue::enqueue_message(uint8_t *message, size_t message_len, bool set_marker)
{
    if (message == nullptr)
    {
      LOG_ERROR("Tried to enqueue nullptr");
      return RTP_INVALID_VALUE;
    }

    if (message_len == 0)
    {
      LOG_ERROR("Tried to enqueue zero length message");
      return RTP_INVALID_VALUE;
    }

    /* Create buffer vector where the full packet is constructed
     * and which is then pushed to "active_"'s pkt_vec structure */
    uvgrtp::buf_vec tmp;

    /* update the RTP header at "rtpheaders_ptr_" */
    uvgrtp::frame_queue::update_rtp_header();

    if (set_marker)
        ((uint8_t *)&active_->rtp_headers[active_->rtphdr_ptr])[1] |= (1 << 7);

    /* Push RTP header first and then push all payload buffers */
    tmp.push_back({
        sizeof(active_->rtp_headers[active_->rtphdr_ptr]),
        (uint8_t *)&active_->rtp_headers[active_->rtphdr_ptr++]
    });

    /* If SRTP with proper encryption has been enabled but
     * RCE_SRTP_INPLACE_ENCRYPTION has **not** been enabled, make a copy of the memory block*/
    if ((flags_ & (RCE_SRTP | RCE_SRTP_INPLACE_ENCRYPTION | RCE_SRTP_NULL_CIPHER)) == RCE_SRTP)
        message = (uint8_t *)memdup(message, message_len);

    tmp.push_back({ message_len, message });

    if (flags_ & RCE_SRTP_AUTHENTICATE_RTP) {
        tmp.push_back({
            UVG_AUTH_TAG_LENGTH,
            (uint8_t *)&active_->rtp_auth_tags[10 * active_->rtpauth_ptr++]
        });
    }

    active_->packets.push_back(tmp);
    rtp_->inc_sequence();
    rtp_->inc_sent_pkts();

    return RTP_OK;
}

rtp_error_t uvgrtp::frame_queue::enqueue_message(uint8_t *message, size_t message_len)
{
    return enqueue_message(message, message_len, false);
}

rtp_error_t uvgrtp::frame_queue::enqueue_message(std::vector<std::pair<size_t, uint8_t *>>& buffers)
{
    if (!buffers.size())
    {
        LOG_ERROR("Tried to enqueue an empty buffer");
        return RTP_INVALID_VALUE;
    }

    /* Create buffer vector where the full packet is constructed
     * and which is then pushed to "active_"'s pkt_vec structure */
    uvgrtp::buf_vec tmp;

    /* update the RTP header at "rtpheaders_ptr_" */
    uvgrtp::frame_queue::update_rtp_header();

    /* Push RTP header first and then push all payload buffers */
    tmp.push_back({
        sizeof(active_->rtp_headers[active_->rtphdr_ptr]),
        (uint8_t *)&active_->rtp_headers[active_->rtphdr_ptr++]
    });

    /* If SRTP with proper encryption is used and there are more than one buffer,
     * frame queue must be a copy of the input and  */
    if ((flags_ & RCE_SRTP) && !(flags_ & RCE_SRTP_NULL_CIPHER) && buffers.size() > 1) {
        size_t total = 0;
        uint8_t *mem = nullptr;
        uint8_t *ptr = nullptr;

        for (auto& buffer : buffers)
            total += buffer.first;

        mem = ptr = new uint8_t[total];

        for (auto& buffer : buffers) {
            memcpy(ptr, buffer.second, buffer.first);
            ptr += buffer.first;
        }

        tmp.push_back({ total, mem });

    } else {
        for (auto& buffer : buffers)
            tmp.push_back({ buffer.first, buffer.second });
    }

    if (flags_ & RCE_SRTP_AUTHENTICATE_RTP) {
        tmp.push_back({
            UVG_AUTH_TAG_LENGTH,
            (uint8_t *)&active_->rtp_auth_tags[10 * active_->rtpauth_ptr++]
        });
    }

    active_->packets.push_back(tmp);
    rtp_->inc_sequence();
    rtp_->inc_sent_pkts();

    return RTP_OK;
}

rtp_error_t uvgrtp::frame_queue::flush_queue()
{
    if (active_->packets.empty()) {
        LOG_ERROR("Cannot send an empty packet!");
        (void)deinit_transaction();
        return RTP_INVALID_VALUE;
    }

    /* set the marker bit of the last packet to 1 */
    if (active_->packets.size() > 1)
        ((uint8_t *)&active_->rtp_headers[active_->rtphdr_ptr - 1])[1] |= (1 << 7);

    transaction_mtx_.lock();
    queued_.insert(std::make_pair(active_->key, active_));
    transaction_mtx_.unlock();

    if (socket_->sendto(active_->packets, 0) != RTP_OK) {
        LOG_ERROR("Failed to flush the message queue: %s", strerror(errno));
        (void)deinit_transaction();
        return RTP_SEND_ERROR;
    }

    //LOG_DEBUG("full message took %zu chunks and %zu messages", active_->chunk_ptr, active_->hdr_ptr);
    return deinit_transaction();
}

void uvgrtp::frame_queue::update_rtp_header()
{
    memcpy(&active_->rtp_headers[active_->rtphdr_ptr], &active_->rtp_common, sizeof(active_->rtp_common));
    rtp_->update_sequence((uint8_t *)(&active_->rtp_headers[active_->rtphdr_ptr]));
}

uvgrtp::buf_vec& uvgrtp::frame_queue::get_buffer_vector()
{
    return active_->buffers;
}

void *uvgrtp::frame_queue::get_media_headers()
{
    return active_->media_headers;
}

uint8_t *uvgrtp::frame_queue::get_active_dataptr()
{
    if (!active_)
        return nullptr;

    if (active_->data_smart)
        return active_->data_smart.get();
    return active_->data_raw;
}

void uvgrtp::frame_queue::install_dealloc_hook(void (*dealloc_hook)(void *))
{
    if (!dealloc_hook)
        return;

    dealloc_hook_ = dealloc_hook;
}
