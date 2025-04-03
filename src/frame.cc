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

rtp_error_t uvgrtp::frame::dealloc_sr(uvgrtp::frame::rtcp_sr* sr)
{
    if (!sr)
        return RTP_INVALID_VALUE;

    if (sr->report_blocks) {
        delete[] sr->report_blocks;
    }

    return RTP_OK;
}

rtp_error_t uvgrtp::frame::dealloc_rr(uvgrtp::frame::rtcp_rr* rr)
{
    if (!rr)
        return RTP_INVALID_VALUE;

    if (rr->report_blocks) {
        delete[] rr->report_blocks;
    }

    return RTP_OK;
}

rtp_error_t uvgrtp::frame::dealloc_sdes(uvgrtp::frame::rtcp_sdes* sdes)
{
    if (!sdes)
        return RTP_INVALID_VALUE;

    if (sdes->chunks) {
        // Iterate through chunks and deallocate items
        for (size_t i = 0; i < sdes->header.count; ++i) {  // Use header.sc to determine chunk count
            if (sdes->chunks[i].items) {
                delete[] sdes->chunks[i].items;
            }
        }

        // Finally, delete the chunks array itself
        delete[] sdes->chunks;
    }

    return RTP_OK;
}