#include "uvgrtp/frame.hh"

#include "uvgrtp/util.hh"

#include "debug.hh"

#include <cstring>


uvgrtp::frame::rtp_frame *uvgrtp::frame::alloc_rtp_frame()
{
    uvgrtp::frame::rtp_frame *frame = new uvgrtp::frame::rtp_frame;

    frame->header.version   = 0;
    frame->header.padding   = 0;
    frame->header.ext       = 0;
    frame->header.cc        = 0;
    frame->header.marker    = 0;
    frame->header.payload   = 0;
    frame->header.seq       = 0;
    frame->header.timestamp = 0;
    frame->header.ssrc      = 0;

    frame->payload   = nullptr;

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

rtp_error_t uvgrtp::frame::dealloc_frame(uvgrtp::frame::rtp_frame *frame)
{
    if (!frame)
        return RTP_INVALID_VALUE;

    if (frame->csrc)
        delete[] frame->csrc;

    if (frame->ext) {
        delete[] frame->ext->data;
        delete frame->ext;
    }

    else if (frame->payload)
        delete[] frame->payload;

    //UVG_LOG_DEBUG("Deallocating frame, type %u", frame->type);

    delete frame;
    return RTP_OK;
}

void* uvgrtp::frame::alloc_zrtp_frame(size_t size)
{
    if (size == 0) {
        rtp_errno = RTP_INVALID_VALUE;
        return nullptr;
    }

    //UVG_LOG_DEBUG("Allocate ZRTP frame, packet size %zu", size);

    uvgrtp::frame::zrtp_frame *frame = (uvgrtp::frame::zrtp_frame *)new uint8_t[size];

    if (frame == nullptr) {
        rtp_errno = RTP_MEMORY_ERROR;
        return nullptr;
    }

    return frame;
}

rtp_error_t uvgrtp::frame::dealloc_frame(uvgrtp::frame::zrtp_frame* frame)
{
    if (!frame)
        return RTP_INVALID_VALUE;

    delete[] frame;
    return RTP_OK;
}
