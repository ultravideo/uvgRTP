#include <cstring>

#include "debug.hh"
#include "frame.hh"
#include "util.hh"

uvg_rtp::frame::rtp_frame *uvg_rtp::frame::alloc_rtp_frame()
{
    uvg_rtp::frame::rtp_frame *frame = new uvg_rtp::frame::rtp_frame;

    if (!frame) {
        rtp_errno = RTP_MEMORY_ERROR;
        return nullptr;
    }

    std::memset(&frame->header, 0, sizeof(uvg_rtp::frame::rtp_header));
    std::memset(frame,          0, sizeof(uvg_rtp::frame::rtp_frame));

    frame->payload   = nullptr;
    frame->probation = nullptr;

    return frame;
}

uvg_rtp::frame::rtp_frame *uvg_rtp::frame::alloc_rtp_frame(size_t payload_len)
{
    uvg_rtp::frame::rtp_frame *frame = nullptr;

    if ((frame = uvg_rtp::frame::alloc_rtp_frame()) == nullptr)
        return nullptr;

    frame->payload     = new uint8_t[payload_len];
    frame->payload_len = payload_len;

    return frame;
}

uvg_rtp::frame::rtp_frame *uvg_rtp::frame::alloc_rtp_frame(size_t payload_len, size_t pz_size)
{
    uvg_rtp::frame::rtp_frame *frame = nullptr;

    if ((frame = uvg_rtp::frame::alloc_rtp_frame()) == nullptr)
        return nullptr;

    frame->probation     = new uint8_t[pz_size * MAX_PAYLOAD + payload_len];
    frame->probation_len = pz_size * MAX_PAYLOAD;
    frame->probation_off = 0;

    frame->payload     = (uint8_t *)frame->probation + frame->probation_len;
    frame->payload_len = payload_len;

    return frame;
}

rtp_error_t uvg_rtp::frame::dealloc_frame(uvg_rtp::frame::rtp_frame *frame)
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

    LOG_DEBUG("Deallocating frame, type %u", frame->type);

    delete frame;
    return RTP_OK;
}

uvg_rtp::frame::zrtp_frame *uvg_rtp::frame::alloc_zrtp_frame(size_t size)
{
    if (size == 0) {
        rtp_errno = RTP_INVALID_VALUE;
        return nullptr;
    }

    LOG_DEBUG("Allocate ZRTP frame, packet size %zu", size);

    uvg_rtp::frame::zrtp_frame *frame = (uvg_rtp::frame::zrtp_frame *)new uint8_t[size];

    if (frame == nullptr) {
        rtp_errno = RTP_MEMORY_ERROR;
        return nullptr;
    }

    return frame;
}

rtp_error_t uvg_rtp::frame::dealloc_frame(uvg_rtp::frame::zrtp_frame *frame)
{
    if (!frame)
        return RTP_INVALID_VALUE;

    delete[] frame;
    return RTP_OK;
}

uvg_rtp::frame::rtcp_sender_frame *uvg_rtp::frame::alloc_rtcp_sender_frame(size_t nblocks)
{
    size_t total_size =
        sizeof(rtcp_header) +
        sizeof(uint32_t) +
        sizeof(rtcp_sender_info) +
        sizeof(rtcp_report_block) * nblocks;

    auto *frame = (uvg_rtp::frame::rtcp_sender_frame *)new uint8_t[total_size];

    if (!frame) {
        LOG_ERROR("Failed to allocate memory for RTCP sender report");
        rtp_errno = RTP_MEMORY_ERROR;
        return nullptr;
    }

    frame->header.version  = 2;
    frame->header.padding  = 0;
    frame->header.pkt_type = uvg_rtp::frame::RTCP_FT_SR;
    frame->header.length   = total_size;
    frame->header.count    = nblocks;

    /* caller fills these */
    memset(&frame->s_info, 0, sizeof(rtcp_sender_info));

    if (nblocks == 0)
        memset(frame->blocks,  0, sizeof(rtcp_report_block) * nblocks);

    return frame;
}

uvg_rtp::frame::rtcp_bye_frame *uvg_rtp::frame::alloc_rtcp_bye_frame(size_t ssrc_count)
{
    if (ssrc_count == 0) {
        LOG_ERROR("Cannot have 0 SSRC/CSRC!");
        rtp_errno = RTP_INVALID_VALUE;
        return nullptr;
    }

    size_t total_size = sizeof(rtcp_header) + sizeof(uint32_t) * ssrc_count;
    auto *frame       = (uvg_rtp::frame::rtcp_bye_frame *)new uint8_t[total_size];

    if (!frame) {
        LOG_ERROR("Failed to allocate memory for RTCP sender report");
        rtp_errno = RTP_MEMORY_ERROR;
        return nullptr;
    }

    frame->header.version  = 2;
    frame->header.padding  = 0;
    frame->header.pkt_type = uvg_rtp::frame::RTCP_FT_BYE;
    frame->header.length   = total_size;
    frame->header.count    = ssrc_count;

    return frame;
}

rtp_error_t uvg_rtp::frame::dealloc_frame(uvg_rtp::frame::rtcp_sender_frame *frame)
{
    if (!frame)
        return RTP_INVALID_VALUE;

    delete[] frame;
    return RTP_OK;
}

rtp_error_t uvg_rtp::frame::dealloc_frame(rtcp_bye_frame *frame)
{
    if (!frame)
        return RTP_INVALID_VALUE;

    delete[] frame;
    return RTP_OK;
}
