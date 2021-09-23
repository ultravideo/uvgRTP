#include "frame.hh"

#include "util.hh"
#include "debug.hh"

#include <cstring>


uvgrtp::frame::rtp_frame *uvgrtp::frame::alloc_rtp_frame()
{
    uvgrtp::frame::rtp_frame *frame = new uvgrtp::frame::rtp_frame;

    std::memset(&frame->header, 0, sizeof(uvgrtp::frame::rtp_header));
    std::memset(frame,          0, sizeof(uvgrtp::frame::rtp_frame));

    frame->payload   = nullptr;
    frame->probation = nullptr;

    return frame;
}

uvgrtp::frame::rtp_frame *uvgrtp::frame::alloc_rtp_frame(size_t payload_len)
{
    uvgrtp::frame::rtp_frame *frame = nullptr;

    if ((frame = uvgrtp::frame::alloc_rtp_frame()) == nullptr)
        return nullptr;

    frame->payload     = new uint8_t[payload_len];
    frame->payload_len = payload_len;

    return frame;
}

uvgrtp::frame::rtp_frame *uvgrtp::frame::alloc_rtp_frame(size_t payload_len, size_t pz_size)
{
    uvgrtp::frame::rtp_frame *frame = nullptr;

    if ((frame = uvgrtp::frame::alloc_rtp_frame()) == nullptr)
        return nullptr;

    frame->probation     = new uint8_t[pz_size * MAX_PAYLOAD + payload_len];
    frame->probation_len = pz_size * MAX_PAYLOAD;
    frame->probation_off = 0;

    frame->payload     = (uint8_t *)frame->probation + frame->probation_len;
    frame->payload_len = payload_len;

    return frame;
}

rtp_error_t uvgrtp::frame::dealloc_frame(uvgrtp::frame::rtp_frame *frame)
{
    if (!frame)
        return RTP_INVALID_VALUE;

    if (frame->csrc)
        delete[] frame->csrc;

    if (frame->ext)
        delete frame->ext;

    if (frame->probation)
        delete[] frame->probation;

    else if (frame->payload)
        delete[] frame->payload;

    //LOG_DEBUG("Deallocating frame, type %u", frame->type);

    delete frame;
    return RTP_OK;
}

uvgrtp::frame::zrtp_frame *uvgrtp::frame::alloc_zrtp_frame(size_t size)
{
    if (size == 0) {
        rtp_errno = RTP_INVALID_VALUE;
        return nullptr;
    }

    LOG_DEBUG("Allocate ZRTP frame, packet size %zu", size);

    uvgrtp::frame::zrtp_frame *frame = (uvgrtp::frame::zrtp_frame *)new uint8_t[size];

    if (frame == nullptr) {
        rtp_errno = RTP_MEMORY_ERROR;
        return nullptr;
    }

    return frame;
}

rtp_error_t uvgrtp::frame::dealloc_frame(uvgrtp::frame::zrtp_frame *frame)
{
    if (!frame)
        return RTP_INVALID_VALUE;

    delete[] frame;
    return RTP_OK;
}
