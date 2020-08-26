#ifdef _WIN32
#else
#endif

#include "rtcp.hh"

rtp_error_t uvg_rtp::rtcp::handle_bye_packet(uint8_t *frame, size_t size)
{
#if 0
    (void)size;

    if (!frame)
        return RTP_INVALID_VALUE;

    for (size_t i = 0; i < frame->header.count; ++i) {
        uint32_t ssrc = ntohl(frame->ssrc[i]);

        if (!is_participant(ssrc)) {
            LOG_WARN("Participants 0x%x is not part of this group!", ssrc);
            continue;
        }

        delete participants_[ssrc]->socket;
        delete participants_[ssrc];
        participants_.erase(ssrc);
    }
#endif

    return RTP_OK;
}

rtp_error_t uvg_rtp::rtcp::send_bye_packet(uvg_rtp::frame::rtcp_bye_frame *frame)
{
    if (!frame)
        return RTP_INVALID_VALUE;

    if (frame->header.count == 0) {
        LOG_WARN("Source Count in RTCP BYE packet is 0");
    }

    uint16_t len         = frame->header.length;
    frame->header.length = htons(frame->header.length);

    for (size_t i = 0; i < frame->header.count; ++i) {
        frame->ssrc[i] = htonl(frame->ssrc[i]);
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
