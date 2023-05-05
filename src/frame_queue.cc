#include "frame_queue.hh"

#include "formats/h264.hh"
#include "formats/h265.hh"
#include "formats/h266.hh"

#include "rtp.hh"
#include "srtp/base.hh"

#include "random.hh"
#include "debug.hh"
#include <thread>

#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#include <ws2def.h>
#include <ws2ipdef.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <cassert>
#include <cstring>
#endif


uvgrtp::frame_queue::frame_queue(std::shared_ptr<uvgrtp::socket> socket, std::shared_ptr<uvgrtp::rtp> rtp, int rce_flags):
    active_(nullptr),
    dealloc_hook_(nullptr),
    max_mcount_(MAX_MSG_COUNT),
    max_ccount_(MAX_CHUNK_COUNT* max_mcount_),
    rtp_(rtp), 
    socket_(socket),
    rce_flags_(rce_flags),
    fps_(false),
    frame_interval_(),
    fps_sync_point_(),
    frames_since_sync_(0)
{}

uvgrtp::frame_queue::~frame_queue()
{
    if (active_)
    {
        (void)deinit_transaction();
    }
}

rtp_error_t uvgrtp::frame_queue::init_transaction()
{
    if (active_)
    {
        (void)deinit_transaction();
    }

    active_      = new transaction_t;

#ifndef _WIN32
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

    active_->hdr_ptr     = 0;
    active_->rtphdr_ptr  = 0;
    active_->rtpauth_ptr = 0;

    active_->data_raw     = nullptr;
    active_->data_smart   = nullptr;
    active_->dealloc_hook = dealloc_hook_;

    if (rce_flags_ & RCE_SRTP_AUTHENTICATE_RTP)
        active_->rtp_auth_tags = new uint8_t[10 * max_mcount_];
    else
        active_->rtp_auth_tags = nullptr;

    rtp_->fill_header((uint8_t *)&active_->rtp_common);
    active_->buffers.clear();

    return RTP_OK;
}

rtp_error_t uvgrtp::frame_queue::init_transaction(uint8_t *data)
{
    if (!data)
        return RTP_INVALID_VALUE;

    if (init_transaction() != RTP_OK) {
        UVG_LOG_ERROR("Failed to initialize transaction");
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
        UVG_LOG_ERROR("Failed to initialize transaction");
        return RTP_GENERIC_ERROR;
    }

    /* The transaction has been initialized to "active_" */
    active_->data_smart = std::move(data);

    return RTP_OK;
}

rtp_error_t uvgrtp::frame_queue::deinit_transaction()
{
    if (active_ == nullptr) {
        UVG_LOG_WARN("Trying to deinit transaction, no active transaction!");
        return RTP_INVALID_VALUE;
    }

    if (active_->headers)
        delete[] active_->headers;

    if (active_->chunks)
        delete[] active_->chunks;

    if (active_->rtp_headers)
        delete[] active_->rtp_headers;

    if (active_->rtp_auth_tags)
        delete[] active_->rtp_auth_tags;

    active_->headers = nullptr;
    active_->chunks = nullptr;
    active_->rtp_headers = nullptr;
    active_->rtp_auth_tags = nullptr;

    if (active_->media_headers)
    {
        switch (rtp_->get_payload()) {
        case RTP_FORMAT_H264:
            delete (uvgrtp::formats::h264_headers*)active_->media_headers;
            active_->media_headers = nullptr;
            break;

        case RTP_FORMAT_H265:
            delete (uvgrtp::formats::h265_headers*)active_->media_headers;
            active_->media_headers = nullptr;
            break;

        case RTP_FORMAT_H266:
            delete (uvgrtp::formats::h266_headers*)active_->media_headers;
            active_->media_headers = nullptr;
            break;

        default:
            break;
        }
    }

    delete active_;
    active_ = nullptr;

    return RTP_OK;
}

rtp_error_t uvgrtp::frame_queue::enqueue_message(uint8_t *message, size_t message_len, bool set_m_bit)
{
    if (message == nullptr || message_len == 0)
    {
        UVG_LOG_ERROR("Tried to enqueue invalid message");
      return RTP_INVALID_VALUE;
    }

    /* Create buffer vector where the full packet is constructed
     * and which is then pushed to "active_"'s pkt_vec structure */
    uvgrtp::buf_vec tmp;

    /* update the RTP header at "rtpheaders_ptr_" */
    update_rtp_header();

    if (set_m_bit)
        ((uint8_t *)&active_->rtp_headers[active_->rtphdr_ptr])[1] |= (1 << 7);

    /* Push RTP header first and then push all payload buffers */
    tmp.push_back({
        sizeof(active_->rtp_headers[active_->rtphdr_ptr]),
        (uint8_t *)&active_->rtp_headers[active_->rtphdr_ptr++]
    });

    tmp.push_back({ message_len, message });

    enqueue_finalize(tmp);
    return RTP_OK;
}

rtp_error_t uvgrtp::frame_queue::enqueue_message(uint8_t *message, size_t message_len)
{
    return enqueue_message(message, message_len, false);
}

rtp_error_t uvgrtp::frame_queue::enqueue_message(buf_vec& buffers)
{
    if (!buffers.size())
    {
        UVG_LOG_ERROR("Tried to enqueue an empty buffer");
        return RTP_INVALID_VALUE;
    }

    /* update the RTP header at "rtpheaders_ptr_" */
    update_rtp_header();

    /* Create buffer vector where the full packet is constructed
     * and which is then pushed to "active_"'s pkt_vec structure */
    uvgrtp::buf_vec tmp;

    /* Push RTP header first and then push all payload buffers */
    tmp.push_back({     sizeof(active_->rtp_headers[active_->rtphdr_ptr]), 
                   (uint8_t *)&active_->rtp_headers[active_->rtphdr_ptr++]});

    /* If SRTP with proper encryption is used and there are more than one buffer,
     * frame queue must be a copy of the input and ... */
    if ((rce_flags_ & RCE_SRTP) && !(rce_flags_ & RCE_SRTP_NULL_CIPHER) && buffers.size() > 1) {
        
        size_t total = 0;

        for (auto& buffer : buffers) {
            total += buffer.first;
        }

        uint8_t* mem = new uint8_t[total];
        uint8_t* ptr = mem;

        // copy buffers to a single pointer
        for (auto& buffer : buffers) {
            memcpy(ptr, buffer.second, buffer.first);
            ptr += buffer.first;
        }

        tmp.push_back({ total, mem });

    } else {
        for (auto& buffer : buffers) {
            tmp.push_back({ buffer.first, buffer.second });
        }
    }

    enqueue_finalize(tmp);
    return RTP_OK;
}

rtp_error_t uvgrtp::frame_queue::flush_queue(sockaddr_in& addr, sockaddr_in6& addr6)
{
    if (active_->packets.empty()) {
        UVG_LOG_ERROR("Cannot send an empty packet!");
        (void)deinit_transaction();
        return RTP_INVALID_VALUE;
    }

    /* set the marker bit of the last packet to 1 */
    if (active_->packets.size() > 1)
        ((uint8_t *)&active_->rtp_headers[active_->rtphdr_ptr - 1])[1] |= (1 << 7);
    
    std::chrono::high_resolution_clock::time_point now = std::chrono::high_resolution_clock::now();

    if ((rce_flags_ & RCE_FRAME_RATE) && fps_)
    {
        std::chrono::nanoseconds wait_time = this_frame_time() - now;

        if (wait_time.count() < 0 || force_sync_)
        {
            if (wait_time.count() < 0)
            {
                /*
                UVG_LOG_DEBUG("Updating fps synchronization point because we are late by %lli ms", 
                    -std::chrono::duration_cast<std::chrono::milliseconds> (wait_time).count());
                    */
            }
            else if ( wait_time < frame_interval_ * 0.5)
            {
                UVG_LOG_DEBUG("Frames are arriving with sensible delay, ending forced synchronization point update");
                force_sync_ = false;
            }
            else
            {
                UVG_LOG_DEBUG("Forcing fps synchronization point update");
            }
            update_sync_point();
        }
        else
        {
            // we cap the sleep/latency at frame interval
            if (wait_time > frame_interval_)
            {
                UVG_LOG_DEBUG("Limiting fps wait times to frame interval");
                std::this_thread::sleep_for(frame_interval_);

                update_sync_point();
            }
            else
            {
                // if nothing is wrong, wait until it is time to send this frame
                std::this_thread::sleep_for(wait_time);
            }
            now = std::chrono::high_resolution_clock::now(); // update now in case we are using fragment pacing
        }

        ++frames_since_sync_;
    }

    if ((rce_flags_ & RCE_PACE_FRAGMENT_SENDING) && fps_ && !force_sync_)
    {
        // allocate 80% of frame interval for pacing, rest for other processing
        std::chrono::nanoseconds packet_interval = 8*frame_interval_/(10*active_->packets.size());

        for (size_t i = 0; i < active_->packets.size(); ++i)
        {
            std::chrono::high_resolution_clock::time_point next_packet = now + i * packet_interval;

            // sleep until next packet time
            std::this_thread::sleep_for(next_packet - std::chrono::high_resolution_clock::now());

            //  send pkt vects
            if (socket_->sendto(addr, addr6, active_->packets[i], 0) != RTP_OK) {
                UVG_LOG_ERROR("Failed to send packet: %li", errno);
                (void)deinit_transaction();
                return RTP_SEND_ERROR;
            }
        }

    }
    else if (socket_->sendto(addr, addr6, active_->packets, 0) != RTP_OK) {
        UVG_LOG_ERROR("Failed to flush the message queue: %li", errno);
        (void)deinit_transaction();
        return RTP_SEND_ERROR;
    }

    //UVG_LOG_DEBUG("full message took %zu chunks and %zu messages", active_->chunk_ptr, active_->hdr_ptr);
    return deinit_transaction();
}

inline std::chrono::high_resolution_clock::time_point uvgrtp::frame_queue::this_frame_time()
{
    return fps_sync_point_ +
        std::chrono::nanoseconds((uint64_t)(frames_since_sync_ * frame_interval_.count()));
}

void uvgrtp::frame_queue::update_rtp_header()
{
    memcpy(&active_->rtp_headers[active_->rtphdr_ptr], &active_->rtp_common, sizeof(active_->rtp_common));
    rtp_->update_sequence((uint8_t *)(&active_->rtp_headers[active_->rtphdr_ptr]));
}

uvgrtp::buf_vec* uvgrtp::frame_queue::get_buffer_vector()
{
    if (!active_)
    {
        UVG_LOG_ERROR("No active transaction");
        return nullptr;
    }
  
    return &active_->buffers;
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

void uvgrtp::frame_queue::enqueue_finalize(uvgrtp::buf_vec& tmp)
{
    if (rce_flags_ & RCE_SRTP_AUTHENTICATE_RTP) {
        tmp.push_back({
            UVG_AUTH_TAG_LENGTH,
            (uint8_t*)&active_->rtp_auth_tags[10 * active_->rtpauth_ptr++]
            });
    }

    active_->packets.push_back(tmp);
    rtp_->inc_sequence();
    rtp_->inc_sent_pkts();
}

inline void uvgrtp::frame_queue::update_sync_point()
{
    //UVG_LOG_DEBUG("Updating framerate sync point");
    frames_since_sync_ = 0;
    fps_sync_point_ = std::chrono::high_resolution_clock::now();
}