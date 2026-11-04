#include "media.hh"

#include "../socket.hh"
#include "../rtp.hh"
#include "../frame_queue.hh"
#include "debug.hh"
#include "../frame_internal.hh"
#include "../global.hh"

#include <map>
#include <unordered_map>



#define INVALID_SEQ 0xffffffff

/* Upper bound for generic frames that are being reassembled at the same time */
constexpr size_t MAX_INCOMPLETE_FRAMES = 256;

/* How often incomplete frames are checked for expiry */
constexpr int64_t GC_INTERVAL_MS = 100;

static void free_media_frame(uvgrtp::formats::media_info_t& info)
{
    for (auto& frag : info.fragments) {
        (void)uvgrtp::frame::dealloc_frame(frag.second);
    }
    info.fragments.clear();
}

/* Drop frames whose fragments have not all arrived within the maximum packet delay.
 * Without this, every frame with a missing fragment stayed in memory forever. */
static void garbage_collect_media_frames(uvgrtp::formats::media_frame_info_t* minfo)
{
    auto now = std::chrono::steady_clock::now();
    if (std::chrono::duration_cast<std::chrono::milliseconds>(now - minfo->last_gc).count() < GC_INTERVAL_MS) {
        return;
    }
    minfo->last_gc = now;

    int64_t max_delay = minfo->rtp_ctx ? (int64_t)minfo->rtp_ctx->get_pkt_max_delay() : (int64_t)uvgrtp::PKT_MAX_DELAY_MS;

    for (auto it = minfo->frames.begin(); it != minfo->frames.end();) {
        if (std::chrono::duration_cast<std::chrono::milliseconds>(now - it->second.start).count() > max_delay) {
            UVG_LOG_DEBUG("Dropping incomplete generic frame, ts %u", it->first);
            free_media_frame(it->second);
            it = minfo->frames.erase(it);
        }
        else {
            ++it;
        }
    }
}

uvgrtp::formats::media::media(std::shared_ptr<uvgrtp::socket> socket, std::shared_ptr<uvgrtp::rtp> rtp_ctx, int rce_flags):
    socket_(socket), rtp_ctx_(rtp_ctx), rce_flags_(rce_flags), fqueue_(new uvgrtp::frame_queue(socket, rtp_ctx, rce_flags)), minfo_()
{
    minfo_.rtp_ctx = rtp_ctx;
    minfo_.last_gc = std::chrono::steady_clock::now();
}

uvgrtp::formats::media::~media()
{
    // Deallocate any remaining fragmented RTP frames stored in minfo_
    for (auto &entry : minfo_.frames) {
        for (auto &frag : entry.second.fragments) {
            (void)uvgrtp::frame::dealloc_frame(frag.second);
        }
        entry.second.fragments.clear();
    }
    minfo_.frames.clear();

    fqueue_ = nullptr;
}

rtp_error_t uvgrtp::formats::media::push_frame(sockaddr_in& addr, sockaddr_in6& addr6,
    uint8_t *data, size_t data_len, int rtp_flags, uint32_t ssrc)
{
    if (!data || !data_len)
        return RTP_INVALID_VALUE;

    return push_media_frame(addr, addr6, data, data_len, rtp_flags, ssrc);
}

rtp_error_t uvgrtp::formats::media::push_frame(sockaddr_in& addr, sockaddr_in6& addr6,
    std::unique_ptr<uint8_t[]> data, size_t data_len, int rtp_flags, uint32_t ssrc)
{
    if (!data || !data_len)
        return RTP_INVALID_VALUE;

    return push_media_frame(addr, addr6, data.get(), data_len, rtp_flags, ssrc);
}

rtp_error_t uvgrtp::formats::media::push_media_frame(sockaddr_in& addr, sockaddr_in6& addr6,
    uint8_t *data, size_t data_len, int rtp_flags, uint32_t ssrc)
{
    (void)rtp_flags;

    rtp_error_t ret;

    if ((ret = fqueue_->init_transaction(data)) != RTP_OK) {
        UVG_LOG_ERROR("Invalid frame queue or failed to initialize transaction!");
        return ret;
    }

    // TODO: Some RTP formats use this fragmentation, enable it if support for those formats is added

    bool fragmentation = (rce_flags_ & RCE_FRAGMENT_GENERIC);
    bool set_marker = false;

    if (!fragmentation || data_len <= rtp_ctx_->get_payload_size()) {

        set_marker = fragmentation;

        if (data_len > rtp_ctx_->get_payload_size()) {
            UVG_LOG_WARN("Packet is larger (%zu bytes) than maximum payload size (%zu bytes)",
                    data_len, rtp_ctx_->get_payload_size());
            UVG_LOG_WARN("Consider using RCE_FRAGMENT_GENERIC!");
        }

        if ((ret = fqueue_->enqueue_message(data, data_len, set_marker)) != RTP_OK) {
            UVG_LOG_ERROR("Failed to enqueue message: %d", ret);
            (void)fqueue_->deinit_transaction();
            return ret;
        }

        return fqueue_->flush_queue(addr, addr6, ssrc);
    }

    size_t payload_size = rtp_ctx_->get_payload_size();
    ssize_t data_left   = data_len;
    ssize_t data_pos    = 0;

    while (data_left > (ssize_t)payload_size) {
        if ((ret = fqueue_->enqueue_message(data + data_pos, payload_size, set_marker)) != RTP_OK) {
            UVG_LOG_ERROR("Failed to enqueue packet when fragmenting generic frame");
            return ret;
        }

        data_pos  += payload_size;
        data_left -= payload_size;
        set_marker = false;
    }

    if ((ret = fqueue_->enqueue_message(data + data_pos, data_left, true)) != RTP_OK) {
        UVG_LOG_ERROR("Failed to enqueue packet when fragmenting generic frame");
        return ret;
    }

    return fqueue_->flush_queue(addr, addr6, ssrc);
}

uvgrtp::formats::media_frame_info_t *uvgrtp::formats::media::get_media_frame_info()
{
    return &minfo_;
}

rtp_error_t uvgrtp::formats::media::packet_handler(void* arg, int rce_flags, uint8_t* read_ptr, size_t size, frame::rtp_frame** out)
{
    (void)rce_flags;
    (void)read_ptr;
    (void)size;
    auto minfo   = (uvgrtp::formats::media_frame_info_t *)arg;
    auto frame   = *out;
    uint32_t ts  = frame->header.timestamp;
    uint32_t seq = frame->header.seq;
    uint32_t recv  = 0;

    bool fragmentation = (rce_flags & RCE_FRAGMENT_GENERIC);

    /* If fragmentation of generic frame has not been enabled, we can just return the frame
     * in "out" because RTP packet handler has done all the necessasry stuff for small RTP packets */
    if (!fragmentation)
    {
        return RTP_PKT_READY;
    }

    garbage_collect_media_frames(minfo);

    if (minfo->frames.find(ts) != minfo->frames.end()) {
        uint32_t key = (seq < minfo->frames[ts].s_seq) ? seq + 0x10000 : seq;

        /* A duplicate used to overwrite (and leak) the stored fragment while still being counted in
         * npkts and size, so the frame could never be completed */
        if (minfo->frames[ts].fragments.find(key) != minfo->frames[ts].fragments.end()) {
            UVG_LOG_DEBUG("Duplicate generic fragment, ts %u, seq %u", ts, seq);
            (void)uvgrtp::frame::dealloc_frame(frame);
            *out = nullptr;
            return RTP_GENERIC_ERROR;
        }

        minfo->frames[ts].npkts++;
        minfo->frames[ts].size += frame->payload_len;
        minfo->frames[ts].fragments[key] = frame;

        *out = nullptr;

        if (frame->header.marker)
        {
            minfo->frames[ts].e_seq = seq;
        }
        else if (seq == minfo->frames[ts].s_seq - 1)
        {
            // try to detect if the start seq was ordered wrong
            minfo->frames[ts].s_seq = seq;
        }

        if (minfo->frames[ts].e_seq != INVALID_SEQ && minfo->frames[ts].s_seq != INVALID_SEQ) {
            if (minfo->frames[ts].s_seq > minfo->frames[ts].e_seq)
            {
                recv = UINT16_MAX - minfo->frames[ts].s_seq + minfo->frames[ts].e_seq + (uint32_t)2;
            }
            else
            {
                recv = minfo->frames[ts].e_seq - minfo->frames[ts].s_seq + (uint32_t)1;
            }

            if (recv == minfo->frames[ts].npkts) {
                auto retframe = uvgrtp::frame::alloc_rtp_frame(minfo->frames[ts].size);
                size_t ptr    = 0;

                std::memcpy(&retframe->header, &frame->header, sizeof(frame->header));

                for (auto& frag : minfo->frames[ts].fragments) {
                    std::memcpy(
                        retframe->payload + ptr,
                        frag.second->payload,
                        frag.second->payload_len
                    );
                    ptr += frag.second->payload_len;
                    (void)uvgrtp::frame::dealloc_frame(frag.second);
                }

                minfo->frames.erase(ts);
                (void)uvgrtp::frame::dealloc_frame(*out);
                *out = retframe;
                return RTP_PKT_READY;
            }
        }
    } else {
        if (!(frame->header.marker)) {
            if (minfo->frames.size() >= MAX_INCOMPLETE_FRAMES) {
                UVG_LOG_WARN("Too many incomplete generic frames, dropping packet");
                (void)uvgrtp::frame::dealloc_frame(frame);
                *out = nullptr;
                return RTP_GENERIC_ERROR;
            }

            minfo->frames[ts].start          = std::chrono::steady_clock::now();
            minfo->frames[ts].npkts          = 1;
            minfo->frames[ts].s_seq          = seq;
            minfo->frames[ts].e_seq          = INVALID_SEQ;
            minfo->frames[ts].fragments[seq] = frame;
            minfo->frames[ts].size           = frame->payload_len;
            *out                             = nullptr;
        } else {
            return RTP_PKT_READY; // fragmentation is used, but there was only one packet for this frame
        }
    }

    return RTP_OK;
}

void uvgrtp::formats::media::set_fps(ssize_t numerator, ssize_t denominator)
{
    fqueue_->set_fps(numerator, denominator);
}

void uvgrtp::formats::media::set_pace(ssize_t numerator, ssize_t denominator)
{
    fqueue_->set_pace(numerator, denominator);
}
