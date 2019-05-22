#include "debug.hh"
#include "frame.hh"
#include "send.hh"
#include "util.hh"

kvz_rtp::frame::rtp_frame *kvz_rtp::frame::alloc_frame(size_t payload_len, frame_type_t type)
{
    if (payload_len == 0 || INVALID_FRAME_TYPE(type)) {
        LOG_ERROR("Invalid parameter!");
        return nullptr;
    }

    LOG_DEBUG("Allocating frame. Size %zu, Type %u", payload_len, type);

    kvz_rtp::frame::rtp_frame *frame = new kvz_rtp::frame::rtp_frame;
    size_t header_len;

    switch (type) {
        case FRAME_TYPE_GENERIC:
            header_len = RTP_HEADER_SIZE;
            break;

        /* for now, opus header is not used */
        case FRAME_TYPE_OPUS:
            header_len = RTP_HEADER_SIZE + 0;
            break;

        case FRAME_TYPE_HEVC_FU:
            header_len = RTP_HEADER_SIZE + HEVC_RTP_HEADER_SIZE + HEVC_FU_HEADER_SIZE;
            break;
    }

    if ((frame = new kvz_rtp::frame::rtp_frame) == nullptr)
        return nullptr;

    if ((frame->header = new uint8_t[payload_len + header_len]) == nullptr) {
        delete frame;
        return nullptr;
    }

    frame->header_len = header_len;
    frame->data_len   = payload_len;
    frame->data       = frame->header + header_len;

    return frame;
}

rtp_error_t kvz_rtp::frame::dealloc_frame(kvz_rtp::frame::rtp_frame *frame)
{
    if (!frame)
        return RTP_INVALID_VALUE;

    if (frame->header)
        delete frame->header;

    LOG_DEBUG("deallocating frame, type %u", frame->frame_type);

    delete frame;
    return RTP_OK;
}

/* get pointer to RTP Header or nullptr if frame is invalid */
uint8_t *kvz_rtp::frame::get_rtp_header(kvz_rtp::frame::rtp_frame *frame)
{
    if (!frame || INVALID_FRAME_TYPE(frame->frame_type))
        return nullptr;

    return frame->header;
}

uint8_t *kvz_rtp::frame::get_opus_header(kvz_rtp::frame::rtp_frame *frame)
{
    if (!frame || !frame->header || frame->frame_type != FRAME_TYPE_OPUS)
        return nullptr;

    return frame->header + HEADER_SIZE_RTP;
}

uint8_t *kvz_rtp::frame::get_hevc_rtp_header(kvz_rtp::frame::rtp_frame *frame)
{
    if (!frame || !frame->header || frame->frame_type != FRAME_TYPE_HEVC_FU)
        return nullptr;

    return frame->header + HEADER_SIZE_RTP;
}

uint8_t *kvz_rtp::frame::get_hevc_fu_header(kvz_rtp::frame::rtp_frame *frame)
{
    if (!frame || !frame->header || frame->frame_type != FRAME_TYPE_HEVC_FU)
        return nullptr;

    return frame->header + HEADER_SIZE_RTP + HEADER_SIZE_HEVC_RTP;
}
