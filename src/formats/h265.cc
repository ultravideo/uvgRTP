#ifdef _WIN32
#else
#include <sys/socket.h>
#endif

#include <cstdint>
#include <cstring>
#include <iostream>
#include <unordered_map>
#include <queue>

#include "debug.hh"
#include "queue.hh"

#include "formats/h265.hh"

rtp_error_t uvg_rtp::formats::h265::push_nal_unit(uint8_t *data, size_t data_len, bool more)
{
    if (data_len <= 3)
        return RTP_INVALID_VALUE;

    uint8_t nal_type    = (data[0] >> 1) & 0x3F;
    rtp_error_t ret     = RTP_OK;
    size_t data_left    = data_len;
    size_t data_pos     = 0;
    size_t payload_size = rtp_ctx_->get_payload_size();

#ifdef __linux__
    if (data_len - 3 <= payload_size) {
        if ((ret = fqueue_->enqueue_message(data, data_len)) != RTP_OK) {
            LOG_ERROR("enqeueu failed for small packet");
            return ret;
        }

        if (more)
            return RTP_NOT_READY;
        return fqueue_->flush_queue();
    }

    /* The payload is larger than MTU (1500 bytes) so we must split it into smaller RTP frames
     * Because we don't if the SCD is enabled and thus cannot make any assumptions about the life time
     * of current stack, we need to store NAL and FU headers to the frame queue transaction.
     *
     * This can be done by asking a handle to current transaction's buffer vectors.
     *
     * During Connection initialization, the frame queue was given HEVC as the payload format so the
     * transaction also contains our media-specifi headers [get_media_headers()]. */
    auto buffers = fqueue_->get_buffer_vector();
    auto headers = (uvg_rtp::formats::h265_headers *)fqueue_->get_media_headers();

    headers->nal_header[0] = 49 << 1; /* fragmentation unit */
    headers->nal_header[1] = 1;       /* temporal id */

    headers->fu_headers[0] = (uint8_t)((1 << 7) | nal_type);
    headers->fu_headers[1] = nal_type;
    headers->fu_headers[2] = (uint8_t)((1 << 6) | nal_type);

    buffers.push_back(std::make_pair(sizeof(headers->nal_header), headers->nal_header));
    buffers.push_back(std::make_pair(sizeof(uint8_t),             &headers->fu_headers[0]));
    buffers.push_back(std::make_pair(payload_size,                nullptr));

    data_pos   = uvg_rtp::frame::HEADER_SIZE_H265_NAL;
    data_left -= uvg_rtp::frame::HEADER_SIZE_H265_NAL;

    while (data_left > payload_size) {
        buffers.at(2).first  = payload_size;
        buffers.at(2).second = &data[data_pos];

        if ((ret = fqueue_->enqueue_message(buffers)) != RTP_OK) {
            LOG_ERROR("Queueing the message failed!");
            fqueue_->deinit_transaction();
            return ret;
        }

        data_pos  += payload_size;
        data_left -= payload_size;

        /* from now on, use the FU header meant for middle fragments */
        buffers.at(1).second = &headers->fu_headers[1];
    }

    /* use the FU header meant for the last fragment */
    buffers.at(1).second = &headers->fu_headers[2];

    buffers.at(2).first  = data_left;
    buffers.at(2).second = &data[data_pos];

    if ((ret = fqueue_->enqueue_message(buffers)) != RTP_OK) {
        LOG_ERROR("Failed to send HEVC frame!");
        fqueue_->deinit_transaction();
        return ret;
    }

    if (more)
        return RTP_NOT_READY;
    return fqueue_->flush_queue();
#else
    if (data_len - 3 <= payload_size) {
        LOG_DEBUG("send unfrag size %zu, type %u", data_len, nal_type);

        if ((ret = uvg_rtp::generic::push_frame(sender, data, data_len, 0)) != RTP_OK) {
            LOG_ERROR("Failed to send small packet! %s", strerror(errno));
            return ret;
        }

        if (more)
            return RTP_NOT_READY;
        return RTP_OK;
    }

    const size_t HEADER_SIZE =
        uvg_rtp::frame::HEADER_SIZE_RTP +
        uvg_rtp::frame::HEADER_SIZE_H265_NAL +
        uvg_rtp::frame::HEADER_SIZE_H265_FU;

    uint8_t buffer[HEADER_SIZE + payload_size] = { 0 };

    rtp_ctx_->fill_header(buffer);

    buffer[uvg_rtp::frame::HEADER_SIZE_RTP + 0]  = 49 << 1;            /* fragmentation unit */
    buffer[uvg_rtp::frame::HEADER_SIZE_RTP + 1]  = 1;                  /* TID */
    buffer[uvg_rtp::frame::HEADER_SIZE_RTP +
           uvg_rtp::frame::HEADER_SIZE_H265_NAL] = (1 << 7) | nal_type; /* Start bit + NAL type */

    data_pos   = uvg_rtp::frame::HEADER_SIZE_H265_NAL;
    data_left -= uvg_rtp::frame::HEADER_SIZE_H265_NAL;

    while (data_left > payload_size) {
        memcpy(&buffer[HEADER_SIZE], &data[data_pos], payload_size);

        if ((ret = socket_->send(buffer, sizeof(buffer), 0)) != RTP_OK)
            return ret;

        rtp_ctx_->update_sequence(buffer);

        data_pos  += payload_size;
        data_left -= payload_size;

        /* Clear extra bits */
        buffer[uvg_rtp::frame::HEADER_SIZE_RTP +
               uvg_rtp::frame::HEADER_SIZE_H265_NAL] = nal_type;
    }

    buffer[uvg_rtp::frame::HEADER_SIZE_RTP +
           uvg_rtp::frame::HEADER_SIZE_H265_NAL] = nal_type | (1 << 6); /* set E bit to signal end of data */

    memcpy(&buffer[HEADER_SIZE], &data[data_pos], data_left);

    if ((ret = socket_->sendto(buffer, HEADER_SIZE + data_left, 0)) != RTP_OK)
        return ret;

    if (more)
        return RTP_NOT_READY;
    return RTP_OK;
#endif
}

uvg_rtp::formats::h265::h265(uvg_rtp::socket *socket, uvg_rtp::rtp *rtp, int flags):
    h26x(socket, rtp, flags)
{
}

uvg_rtp::formats::h265::~h265()
{
}

uvg_rtp::formats::h265_frame_info_t *uvg_rtp::formats::h265::get_h265_frame_info()
{
    return &finfo_;
}
