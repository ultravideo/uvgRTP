#ifdef _WIN32
#else
#endif

#include "rtcp.hh"

uvg_rtp::frame::rtcp_sdes_frame *uvg_rtp::rtcp::get_sdes_packet(uint32_t ssrc)
{
    if (participants_.find(ssrc) == participants_.end())
        return nullptr;

    auto frame = participants_[ssrc]->sdes_frame;
    participants_[ssrc]->sdes_frame = nullptr;

    return frame;
}

rtp_error_t uvg_rtp::rtcp::handle_sdes_packet(uvg_rtp::frame::rtcp_sdes_frame *frame, size_t size)
{
    if (!frame)
        return RTP_INVALID_VALUE;

    if (frame->header.count == 0) {
        LOG_ERROR("SDES packet cannot contain 0 fields!");
        return RTP_INVALID_VALUE;
    }

    frame->sender_ssrc = ntohl(frame->sender_ssrc);

    /* We need to make a copy of the frame because right now frame points to RTCP recv buffer
     * Deallocate previous frame if it exists */
    if (participants_[frame->sender_ssrc]->sdes_frame != nullptr)
        (void)uvg_rtp::frame::dealloc_frame(participants_[frame->sender_ssrc]->sdes_frame);

    uint8_t *cpy_frame = new uint8_t[size];
    memcpy(cpy_frame, frame, size);

    if (sdes_hook_)
        sdes_hook_((uvg_rtp::frame::rtcp_sdes_frame *)cpy_frame);
    else
        participants_[frame->sender_ssrc]->sdes_frame = (uvg_rtp::frame::rtcp_sdes_frame *)cpy_frame;

    return RTP_OK;
}

rtp_error_t uvg_rtp::rtcp::send_sdes_packet(uvg_rtp::frame::rtcp_sdes_frame *frame)
{
    if (!frame)
        return RTP_INVALID_VALUE;

    if (frame->header.count == 0) {
        LOG_WARN("");
    }

    uint16_t len = frame->header.length;

    /* rtcp header + ssrc */
    frame->header.length = htons(frame->header.length);
    frame->sender_ssrc = htonl(frame->sender_ssrc);

    for (size_t i = 0; i < frame->header.count; ++i) {
        frame->items[i].length = htons(frame->items[i].length);
    }

    rtp_error_t ret;

    for (auto& participant : participants_) {
        auto p = participant.second;

        if ((ret = p->socket->sendto(p->address, (uint8_t *)frame, len, 0)) != RTP_OK) {
            LOG_ERROR("sendto() failed!");
            return ret;
        }

        update_rtcp_bandwidth(len);
    }

    return ret;
}
