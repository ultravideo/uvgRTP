#include "formats/h266.hh"

#include "rtp.hh"
#include "queue.hh"
#include "frame.hh"
#include "debug.hh"

#include <cstdint>
#include <cstring>
#include <iostream>
#include <unordered_map>
#include <queue>


#ifndef _WIN32
#include <sys/socket.h>
#endif

rtp_error_t uvgrtp::formats::h266::push_nal_unit(uint8_t *data, size_t data_len, bool more)
{
    if (data_len <= 3)
        return RTP_INVALID_VALUE;

    uint8_t nal_type    = (data[1] >> 3) & 0x1f;
    rtp_error_t ret     = RTP_OK;
    size_t data_left    = data_len;
    size_t data_pos     = 0;
    size_t payload_size = rtp_ctx_->get_payload_size();

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
     * During Connection initialization, the frame queue was given VVC as the payload format so the
     * transaction also contains our media-specific headers [get_media_headers()]. */
    auto buffers = fqueue_->get_buffer_vector();
    auto headers = (uvgrtp::formats::h266_headers *)fqueue_->get_media_headers();

    headers->nal_header[0] = data[0];
    headers->nal_header[1] = (29 << 3) | (data[1] & 0x7);

    headers->fu_headers[0] = (uint8_t)((1 << 7) | nal_type);
    headers->fu_headers[1] = nal_type;
    headers->fu_headers[2] = (uint8_t)((1 << 6) | nal_type);

    buffers.push_back(std::make_pair(sizeof(headers->nal_header), headers->nal_header));
    buffers.push_back(std::make_pair(sizeof(uint8_t),             &headers->fu_headers[0]));
    buffers.push_back(std::make_pair(payload_size,                nullptr));

    data_pos   = uvgrtp::frame::HEADER_SIZE_H266_NAL;
    data_left -= uvgrtp::frame::HEADER_SIZE_H266_NAL;

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
        LOG_ERROR("Failed to send VVC frame!");
        fqueue_->deinit_transaction();
        return ret;
    }

    if (more)
        return RTP_NOT_READY;
    return fqueue_->flush_queue();
}

uvgrtp::formats::h266::h266(uvgrtp::socket *socket, uvgrtp::rtp *rtp, int flags):
    h26x(socket, rtp, flags), finfo_{}
{
    finfo_.rtp_ctx = rtp;
}

uvgrtp::formats::h266::~h266()
{
}

uvgrtp::formats::h266_frame_info_t *uvgrtp::formats::h266::get_h266_frame_info()
{
    return &finfo_;
}
