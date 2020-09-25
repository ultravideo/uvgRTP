#ifdef __linux__
#else
#endif

#include <map>
#include <unordered_map>

#include "debug.hh"
#include "formats/media.hh"

#define INVALID_SEQ 0xffffffff

uvg_rtp::formats::media::media(uvg_rtp::socket *socket, uvg_rtp::rtp *rtp_ctx, int flags):
    socket_(socket), rtp_ctx_(rtp_ctx), flags_(flags)
{
    fqueue_ = new uvg_rtp::frame_queue(socket, rtp_ctx, flags);
}

uvg_rtp::formats::media::~media()
{
}

rtp_error_t uvg_rtp::formats::media::push_frame(uint8_t *data, size_t data_len, int flags)
{
    if (!data || !data_len)
        return RTP_INVALID_VALUE;

    return push_media_frame(data, data_len, flags);
}

rtp_error_t uvg_rtp::formats::media::push_frame(std::unique_ptr<uint8_t[]> data, size_t data_len, int flags)
{
    if (!data || !data_len)
        return RTP_INVALID_VALUE;

    return push_media_frame(data.get(), data_len, flags);
}

rtp_error_t uvg_rtp::formats::media::push_media_frame(uint8_t *data, size_t data_len, int flags)
{
    (void)flags;

    rtp_error_t ret;

    if ((ret = fqueue_->init_transaction(data)) != RTP_OK) {
        LOG_ERROR("Invalid frame queue or failed to initialize transaction!");
        return ret;
    }

    /* TODO: Bring back the support for RCE_FRAGMENT_GENERIC
     *       It requires support for modifying the active packet's RTP header,
     *       functionality currently not provided by the frame queue */
    if (data_len > rtp_ctx_->get_payload_size()) {
        if (flags_ & RCE_FRAGMENT_GENERIC) {
            LOG_ERROR("Generic frame fragmentation currently not supported!");
            return RTP_NOT_SUPPORTED;
        }

        LOG_WARN("Payload is too large and will be truncated (%zu vs %zu)",
            data_len, rtp_ctx_->get_payload_size()
        );
    }

    if ((ret = fqueue_->enqueue_message(data, data_len)) != RTP_OK) {
        LOG_ERROR("Failed to enqueue message: %d", ret);
        (void)fqueue_->deinit_transaction();
        return ret;
    }

    return fqueue_->flush_queue();

#if 0
    std::vector<std::pair<size_t, uint8_t *>> buffers;
    size_t payload_size = rtp_ctx_->get_payload_size();
    uint8_t header[uvg_rtp::frame::HEADER_SIZE_RTP];

    /* fill RTP header with our session's values
     * and push the header to the buffer vector to use vectored I/O */
    rtp_ctx_->fill_header(header);
    buffers.push_back(std::make_pair(sizeof(header), header));
    buffers.push_back(std::make_pair(data_len,       data));

    if (data_len > payload_size) {
        if (flags_ & RCE_FRAGMENT_GENERIC) {

            rtp_error_t ret   = RTP_OK;
            ssize_t data_left = data_len;
            ssize_t data_pos  = 0;

            /* set marker bit for the first fragment */
            header[1] |= (1 << 7);

            while (data_left > (ssize_t)payload_size) {
                buffers.at(1).first  = payload_size;
                buffers.at(1).second = data + data_pos;

                if ((ret = socket_->sendto(buffers, 0)) != RTP_OK)
                    return ret;

                rtp_ctx_->inc_sequence();
                rtp_ctx_->update_sequence(header);

                data_pos  += payload_size;
                data_left -= payload_size;

                /* clear marker bit for middle fragments */
                header[1] &= 0x7f;
            }

            /* set marker bit for the last frame */
            header[1] |= (1 << 7);

            buffers.at(1).first  = data_left;
            buffers.at(1).second = data + data_pos;

            return socket_->sendto(buffers, 0);

        } else {
            LOG_WARN("Packet is larger (%zu bytes) than payload_size (%zu bytes)", data_len, payload_size);
        }
    }
#endif
}

rtp_error_t uvg_rtp::formats::media::packet_handler(void *arg, int flags, uvg_rtp::frame::rtp_frame **out)
{
    (void)arg;

    struct frame_info {
        uint32_t s_seq;
        uint32_t e_seq;
        size_t npkts;
        size_t size;
        std::map<uint16_t, uvg_rtp::frame::rtp_frame *> fragments;
    };
    static std::unordered_map<uint32_t, frame_info> frames;

    auto frame      = *out;
    uint32_t ts     = frame->header.timestamp;
    uint32_t seq    = frame->header.seq;
    size_t recv     = 0;

    /* If fragmentation of generic frame has not been enabled, we can just return the frame
     * in "out" because RTP packet handler has done all the necessasry stuff for small RTP packets */
    if (!(flags & RCE_FRAGMENT_GENERIC))
        return RTP_PKT_READY;

    if (frames.find(ts) != frames.end()) {
        frames[ts].npkts++;
        frames[ts].fragments[seq] = frame;
        frames[ts].size += frame->payload_len;
        *out = nullptr;

        if (frame->header.marker)
            frames[ts].e_seq = seq;

        if (frames[ts].e_seq != INVALID_SEQ && frames[ts].s_seq != INVALID_SEQ) {
            if (frames[ts].s_seq > frames[ts].e_seq)
                recv = 0xffff - frames[ts].s_seq + frames[ts].e_seq + 2;
            else
                recv = frames[ts].e_seq - frames[ts].s_seq + 1;

            if (recv == frames[ts].npkts) {
                auto retframe = uvg_rtp::frame::alloc_rtp_frame(frames[ts].size);
                size_t ptr    = 0;

                std::memcpy(&retframe->header, &frame->header, sizeof(frame->header));

                for (auto& frag : frames[ts].fragments) {
                    std::memcpy(
                        retframe->payload + ptr,
                        frag.second->payload,
                        frag.second->payload_len
                    );
                    ptr += frag.second->payload_len;
                    (void)uvg_rtp::frame::dealloc_frame(frag.second);
                }

                frames.erase(ts);
                (void)uvg_rtp::frame::dealloc_frame(*out);
                *out = retframe;
                return RTP_PKT_READY;
            }
        }
    } else {
        if (frame->header.marker) {
            frames[ts].npkts          = 1;
            frames[ts].s_seq          = seq;
            frames[ts].e_seq          = INVALID_SEQ;
            frames[ts].fragments[seq] = frame;
            frames[ts].size           = frame->payload_len;
            *out                      = nullptr;
        } else {
            return RTP_PKT_READY;
        }
    }

    return RTP_OK;
}
