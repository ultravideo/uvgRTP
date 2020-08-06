#ifdef _WIN32
#else
#endif

#include "rtcp.hh"

uvg_rtp::frame::rtcp_app_frame *uvg_rtp::rtcp::get_app_packet(uint32_t ssrc)
{
    if (participants_.find(ssrc) == participants_.end())
        return nullptr;

    auto frame = participants_[ssrc]->app_frame;
    participants_[ssrc]->app_frame = nullptr;

    return frame;
}

rtp_error_t uvg_rtp::rtcp::handle_app_packet(uvg_rtp::frame::rtcp_app_frame *frame, size_t size)
{
    if (!frame)
        return RTP_INVALID_VALUE;

    frame->ssrc   = ntohl(frame->ssrc);
    frame->length = ntohs(frame->length);

    /* We need to make a copy of the frame because right now frame points to RTCP recv buffer
     * Deallocate previous frame if it exists */
    if (participants_[frame->ssrc]->app_frame != nullptr)
        (void)uvg_rtp::frame::dealloc_frame(participants_[frame->ssrc]->app_frame);

    uint8_t *cpy_frame = new uint8_t[size];
    memcpy(cpy_frame, frame, size);

    if (app_hook_)
        app_hook_((uvg_rtp::frame::rtcp_app_frame *)cpy_frame);
    else
        participants_[frame->ssrc]->app_frame = (uvg_rtp::frame::rtcp_app_frame *)cpy_frame;

    return RTP_OK;
}

rtp_error_t uvg_rtp::rtcp::send_app_packet(uvg_rtp::frame::rtcp_app_frame *frame)
{
    if (!frame)
        return RTP_INVALID_VALUE;

    uint16_t len  = frame->length;
    uint32_t ssrc = frame->ssrc;

    frame->length = htons(frame->length);
    frame->ssrc   = htonl(frame->ssrc);

    if (!is_participant(ssrc)) {
        LOG_ERROR("Unknown participant 0x%x", ssrc);
        return RTP_INVALID_VALUE;
    }

    rtp_error_t ret = participants_[ssrc]->socket->sendto((uint8_t *)frame, len, 0, NULL);

    if (ret == RTP_OK)
        update_rtcp_bandwidth(len);

    return ret;
}
