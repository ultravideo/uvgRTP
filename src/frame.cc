#include "uvgrtp/frame.hh"

#include "uvgrtp/util.hh"

#include "debug.hh"

#include <cstring>


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

    if (frame->payload)
        delete[] frame->payload;

    //UVG_LOG_DEBUG("Deallocating frame, type %u", frame->type);

    delete frame;
    return RTP_OK;
}

