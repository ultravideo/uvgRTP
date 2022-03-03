#include "media.hh"

#include "../rtp.hh"
#include "../frame_queue.hh"

#include "uvgrtp/socket.hh"
#include "uvgrtp/debug.hh"

#include <map>
#include <unordered_map>



#define INVALID_SEQ 0xffffffff

uvgrtp::formats::media::media(std::shared_ptr<uvgrtp::socket> socket, std::shared_ptr<uvgrtp::rtp> rtp_ctx, int flags):
    socket_(socket), rtp_ctx_(rtp_ctx), flags_(flags), fqueue_(new uvgrtp::frame_queue(socket, rtp_ctx, flags)), minfo_()
{}

uvgrtp::formats::media::~media()
{
    fqueue_ = nullptr;
}

rtp_error_t uvgrtp::formats::media::push_frame(uint8_t *data, size_t data_len, int flags)
{
    if (!data || !data_len)
        return RTP_INVALID_VALUE;

    return push_media_frame(data, data_len, flags);
}

rtp_error_t uvgrtp::formats::media::push_frame(std::unique_ptr<uint8_t[]> data, size_t data_len, int flags)
{
    if (!data || !data_len)
        return RTP_INVALID_VALUE;

    return push_media_frame(data.get(), data_len, flags);
}

rtp_error_t uvgrtp::formats::media::push_media_frame(uint8_t *data, size_t data_len, int flags)
{
    (void)flags;

    rtp_error_t ret;

    if ((ret = fqueue_->init_transaction(data)) != RTP_OK) {
        LOG_ERROR("Invalid frame queue or failed to initialize transaction!");
        return ret;
    }

    if (!(flags_ & RCE_FRAGMENT_GENERIC) || data_len <= rtp_ctx_->get_payload_size()) {
        if (data_len > rtp_ctx_->get_payload_size()) {
            LOG_WARN("Packet is larger (%zu bytes) than maximum payload size (%zu bytes)",
                    data_len, rtp_ctx_->get_payload_size());
            LOG_WARN("Consider using RCE_FRAGMENT_GENERIC!");
        }

        if ((ret = fqueue_->enqueue_message(data, data_len)) != RTP_OK) {
            LOG_ERROR("Failed to enqueue message: %d", ret);
            (void)fqueue_->deinit_transaction();
            return ret;
        }

        return fqueue_->flush_queue();
    }

    size_t payload_size = rtp_ctx_->get_payload_size();
    ssize_t data_left   = data_len;
    ssize_t data_pos    = 0;
    bool set_marker     = true;

    while (data_left > (ssize_t)payload_size) {
        if ((ret = fqueue_->enqueue_message(data + data_pos, payload_size, set_marker)) != RTP_OK) {
            LOG_ERROR("Failed to enqueue packet when fragmenting generic frame");
            return ret;
        }

        data_pos  += payload_size;
        data_left -= payload_size;
        set_marker = false;
    }

    if ((ret = fqueue_->enqueue_message(data + data_pos, data_left, true)) != RTP_OK) {
        LOG_ERROR("Failed to enqueue packet when fragmenting generic frame");
        return ret;
    }

    return fqueue_->flush_queue();
}

uvgrtp::formats::media_frame_info_t *uvgrtp::formats::media::get_media_frame_info()
{
    return &minfo_;
}

rtp_error_t uvgrtp::formats::media::packet_handler(void *arg, int flags, uvgrtp::frame::rtp_frame **out)
{
    auto minfo   = (uvgrtp::formats::media_frame_info_t *)arg;
    auto frame   = *out;
    uint32_t ts  = frame->header.timestamp;
    uint32_t seq = frame->header.seq;
    size_t recv  = 0;

    /* If fragmentation of generic frame has not been enabled, we can just return the frame
     * in "out" because RTP packet handler has done all the necessasry stuff for small RTP packets */
    if (!(flags & RCE_FRAGMENT_GENERIC))
        return RTP_PKT_READY;

    if (minfo->frames.find(ts) != minfo->frames.end()) {
        minfo->frames[ts].npkts++;
        minfo->frames[ts].size += frame->payload_len;

        if (seq < minfo->frames[ts].s_seq)
            minfo->frames[ts].fragments[seq + 0x10000] = frame;
        else
            minfo->frames[ts].fragments[seq] = frame;

        *out = nullptr;

        if (frame->header.marker)
            minfo->frames[ts].e_seq = seq;

        if (minfo->frames[ts].e_seq != INVALID_SEQ && minfo->frames[ts].s_seq != INVALID_SEQ) {
            if (minfo->frames[ts].s_seq > minfo->frames[ts].e_seq)
                recv = 0xffff - minfo->frames[ts].s_seq + minfo->frames[ts].e_seq + 2;
            else
                recv = minfo->frames[ts].e_seq - minfo->frames[ts].s_seq + 1;

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
        if (frame->header.marker) {
            minfo->frames[ts].npkts          = 1;
            minfo->frames[ts].s_seq          = seq;
            minfo->frames[ts].e_seq          = INVALID_SEQ;
            minfo->frames[ts].fragments[seq] = frame;
            minfo->frames[ts].size           = frame->payload_len;
            *out                             = nullptr;
        } else {
            return RTP_PKT_READY;
        }
    }

    return RTP_OK;
}
