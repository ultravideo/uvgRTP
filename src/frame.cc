#include "debug.hh"
#include "frame.hh"
#include "send.hh"
#include "util.hh"

kvz_rtp::frame::rtp_frame *kvz_rtp::frame::alloc_rtp_frame(size_t payload_len, rtp_type_t type)
{
    if (payload_len == 0 || INVALID_FRAME_TYPE(type)) {
        LOG_ERROR("Invalid parameter!");
        return nullptr;
    }

    LOG_DEBUG("Allocating frame. Size %zu, Type %u", payload_len, type);

    kvz_rtp::frame::rtp_frame *frame;
    size_t header_len;

    switch (type) {
        case FRAME_TYPE_GENERIC:
            header_len = kvz_rtp::frame::HEADER_SIZE_RTP;
            break;

        /* for now, opus header is not used */
        case FRAME_TYPE_OPUS:
            header_len = kvz_rtp::frame::HEADER_SIZE_RTP + 0;
            break;

        case FRAME_TYPE_HEVC_FU:
            header_len = kvz_rtp::frame::HEADER_SIZE_RTP
                       + kvz_rtp::frame::HEADER_SIZE_HEVC_NAL
                       + kvz_rtp::frame::HEADER_SIZE_HEVC_FU;
            break;
    }

    if ((frame = new kvz_rtp::frame::rtp_frame) == nullptr) {
        LOG_ERROR("Failed to allocate RTP frame!");
        return nullptr;
    }

    if ((frame->data = new uint8_t[payload_len + header_len]) == nullptr) {
        LOG_ERROR("Failed to allocate paylod for RTP frame");
        delete frame;
        return nullptr;
    }

    frame->header_len  = header_len;
    frame->payload_len = payload_len;
    frame->total_len   = payload_len + header_len;
    frame->payload     = frame->data + header_len;
    frame->type        = type;

    return frame;
}

rtp_error_t kvz_rtp::frame::dealloc_frame(kvz_rtp::frame::rtp_frame *frame)
{
    if (!frame)
        return RTP_INVALID_VALUE;

    if (frame->data)
        delete frame->data;

    LOG_DEBUG("Deallocating frame, type %u", frame->type);

    delete frame;
    return RTP_OK;
}

/* get pointer to RTP Header or nullptr if frame is invalid */
uint8_t *kvz_rtp::frame::get_rtp_header(kvz_rtp::frame::rtp_frame *frame)
{
    if (!frame)
        return nullptr;

    return frame->data;
}

uint8_t *kvz_rtp::frame::get_opus_header(kvz_rtp::frame::rtp_frame *frame)
{
    if (!frame || !frame->data || frame->type != FRAME_TYPE_OPUS)
        return nullptr;

    return frame->data + HEADER_SIZE_RTP;
}

uint8_t *kvz_rtp::frame::get_hevc_rtp_header(kvz_rtp::frame::rtp_frame *frame)
{
    if (!frame || !frame->data || frame->type != FRAME_TYPE_HEVC_FU)
        return nullptr;

    return frame->data + HEADER_SIZE_RTP;
}

uint8_t *kvz_rtp::frame::get_hevc_fu_header(kvz_rtp::frame::rtp_frame *frame)
{
    if (!frame || !frame->data || frame->type != FRAME_TYPE_HEVC_FU)
        return nullptr;

    return frame->data + HEADER_SIZE_RTP + HEADER_SIZE_HEVC_NAL;
}
