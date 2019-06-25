#include <cstring>

#include "debug.hh"
#include "frame.hh"
#include "send.hh"
#include "util.hh"

kvz_rtp::frame::rtp_frame *kvz_rtp::frame::alloc_rtp_frame(size_t payload_len, rtp_type_t type)
{
    if (payload_len == 0 || INVALID_FRAME_TYPE(type)) {
        rtp_errno = RTP_INVALID_VALUE;
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
        rtp_errno = RTP_MEMORY_ERROR;
        return nullptr;
    }

    if ((frame->data = new uint8_t[payload_len + header_len]) == nullptr) {
        rtp_errno = RTP_MEMORY_ERROR;
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
        delete[] frame->data;

    LOG_DEBUG("Deallocating frame, type %u", frame->type);

    delete frame;
    return RTP_OK;
}

kvz_rtp::frame::rtcp_sender_frame *kvz_rtp::frame::alloc_rtcp_sender_frame(size_t nblocks)
{
    if (nblocks == 0) {
        LOG_ERROR("Cannot send 0 report blocks!");
        rtp_errno = RTP_INVALID_VALUE;
        return nullptr;
    }

    size_t total_size =
        sizeof(rtcp_header) +
        sizeof(uint32_t) +
        sizeof(rtcp_sender_info) +
        sizeof(rtcp_report_block) * nblocks;

    auto *frame = (kvz_rtp::frame::rtcp_sender_frame *)new uint8_t[total_size];

    if (!frame) {
        LOG_ERROR("Failed to allocate memory for RTCP sender report");
        rtp_errno = RTP_MEMORY_ERROR;
        return nullptr;
    }

    frame->header.version  = 2;
    frame->header.padding  = 0;
    frame->header.pkt_type = kvz_rtp::frame::FRAME_TYPE_RR;
    frame->header.length   = total_size;
    frame->header.count    = nblocks;

    /* caller fills these */
    memset(&frame->s_info, 0, sizeof(rtcp_sender_info));
    memset(frame->blocks,  0, sizeof(rtcp_report_block) * nblocks);

    return frame;
}

kvz_rtp::frame::rtcp_receiver_frame *kvz_rtp::frame::alloc_rtcp_receiver_frame(size_t nblocks)
{
    if (nblocks == 0) {
        LOG_ERROR("Cannot send 0 report blocks!");
        rtp_errno = RTP_INVALID_VALUE;
        return nullptr;
    }

    size_t total_size =
        sizeof(rtcp_header) +
        sizeof(uint32_t) +
        sizeof(rtcp_report_block) * nblocks;

    auto *frame = (kvz_rtp::frame::rtcp_receiver_frame *)new uint8_t[total_size];

    if (!frame) {
        LOG_ERROR("Failed to allocate memory for RTCP sender report");
        rtp_errno = RTP_MEMORY_ERROR;
        return nullptr;
    }

    frame->header.version  = 2;
    frame->header.padding  = 0;
    frame->header.pkt_type = kvz_rtp::frame::FRAME_TYPE_RR;
    frame->header.length   = total_size;
    frame->header.count    = nblocks;

    /* caller fills these */
    memset(frame->blocks, 0, sizeof(rtcp_report_block) * nblocks);

    return frame;
}

kvz_rtp::frame::rtcp_sdes_frame *kvz_rtp::frame::alloc_rtcp_sdes_frame(size_t ssrc_count, size_t total_len)
{
    if (total_len == 0) {
        LOG_ERROR("Cannot allocate empty SDES packet!");
        rtp_errno = RTP_INVALID_VALUE;
        return nullptr;
    }

    size_t total_size = sizeof(rtcp_header) + total_len;

    auto *frame = (kvz_rtp::frame::rtcp_sdes_frame *)new uint8_t[total_size];

    if (!frame) {
        LOG_ERROR("Failed to allocate memory for RTCP sender report");
        rtp_errno = RTP_MEMORY_ERROR;
        return nullptr;
    }

    frame->header.version  = 2;
    frame->header.padding  = 0;
    frame->header.pkt_type = kvz_rtp::frame::FRAME_TYPE_SDES;
    frame->header.length   = total_size;
    frame->header.count    = ssrc_count;

    /* caller fills these */
    memset(frame->items, 0, total_len);

    return frame;
}

kvz_rtp::frame::rtcp_bye_frame *kvz_rtp::frame::alloc_rtcp_bye_frame(size_t ssrc_count)
{
    if (ssrc_count == 0) {
        LOG_ERROR("Cannot have 0 SSRC/CSRC!");
        rtp_errno = RTP_INVALID_VALUE;
        return nullptr;
    }

    size_t total_size = sizeof(rtcp_header) + sizeof(uint32_t) * ssrc_count;
    auto *frame       = (kvz_rtp::frame::rtcp_bye_frame *)new uint8_t[total_size];

    if (!frame) {
        LOG_ERROR("Failed to allocate memory for RTCP sender report");
        rtp_errno = RTP_MEMORY_ERROR;
        return nullptr;
    }

    frame->header.version  = 2;
    frame->header.padding  = 0;
    frame->header.pkt_type = kvz_rtp::frame::FRAME_TYPE_BYE;
    frame->header.length   = total_size;
    frame->header.count    = ssrc_count;

    return frame;
}

kvz_rtp::frame::rtcp_app_frame *kvz_rtp::frame::alloc_rtcp_app_frame(std::string name, uint8_t subtype, size_t payload_len)
{
    if (name == "" || payload_len == 0) {
        rtp_errno = RTP_INVALID_VALUE;
        return nullptr;
    }

    size_t total_size = sizeof(rtcp_app_frame) + payload_len;
    auto *frame       = (kvz_rtp::frame::rtcp_app_frame *)new uint8_t[total_size];

    if (!frame) {
        LOG_ERROR("Failed to allocate memory for RTCP sender report");
        rtp_errno = RTP_MEMORY_ERROR;
        return nullptr;
    }

    frame->version     = 2;
    frame->padding     = 0;
    frame->pkt_type    = kvz_rtp::frame::FRAME_TYPE_APP;
    frame->pkt_subtype = subtype;
    frame->length      = total_size;

    return frame;
}

rtp_error_t kvz_rtp::frame::dealloc_frame(kvz_rtp::frame::rtcp_sender_frame *frame)
{
    if (!frame)
        return RTP_INVALID_VALUE;

    delete[] frame;
    return RTP_OK;
}

rtp_error_t kvz_rtp::frame::dealloc_frame(kvz_rtp::frame::rtcp_receiver_frame *frame)
{
    if (!frame)
        return RTP_INVALID_VALUE;

    delete[] frame;
    return RTP_OK;
}

rtp_error_t kvz_rtp::frame::dealloc_frame(rtcp_sdes_frame *frame)
{
    if (!frame)
        return RTP_INVALID_VALUE;

    delete[] frame;
    return RTP_OK;
}

rtp_error_t kvz_rtp::frame::dealloc_frame(rtcp_bye_frame *frame)
{
    if (!frame)
        return RTP_INVALID_VALUE;

    delete[] frame;
    return RTP_OK;
}

rtp_error_t kvz_rtp::frame::dealloc_frame(rtcp_app_frame *frame)
{
    if (!frame)
        return RTP_INVALID_VALUE;

    delete[] frame;
    return RTP_OK;
}

uint8_t *kvz_rtp::frame::get_rtp_header(kvz_rtp::frame::rtp_frame *frame)
{
    if (!frame) {
        rtp_errno = RTP_INVALID_VALUE;
        return nullptr;
    }

    return frame->data;
}

uint8_t *kvz_rtp::frame::get_opus_header(kvz_rtp::frame::rtp_frame *frame)
{
    if (!frame || !frame->data || frame->type != FRAME_TYPE_OPUS) {
        rtp_errno = RTP_INVALID_VALUE;
        return nullptr;
    }

    return frame->data + HEADER_SIZE_RTP;
}

uint8_t *kvz_rtp::frame::get_hevc_nal_header(kvz_rtp::frame::rtp_frame *frame)
{
    if (!frame || !frame->data || frame->type != FRAME_TYPE_HEVC_FU) {
        rtp_errno = RTP_INVALID_VALUE;
        return nullptr;
    }

    return frame->data + HEADER_SIZE_RTP;
}

uint8_t *kvz_rtp::frame::get_hevc_fu_header(kvz_rtp::frame::rtp_frame *frame)
{
    if (!frame || !frame->data || frame->type != FRAME_TYPE_HEVC_FU) {
        rtp_errno = RTP_INVALID_VALUE;
        return nullptr;
    }

    return frame->data + HEADER_SIZE_RTP + HEADER_SIZE_HEVC_NAL;
}
