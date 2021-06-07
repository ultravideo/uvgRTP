#include "rtcp.hh"

#include "debug.hh"

rtp_error_t uvgrtp::rtcp::handle_bye_packet(uint8_t *packet, size_t size)
{
    if (!packet || !size)
        return RTP_INVALID_VALUE;

    for (size_t i = 4; i < size; i += sizeof(uint32_t)) {
        uint32_t ssrc = ntohl(*(uint32_t *)&packet[i]);

        if (!is_participant(ssrc)) {
            LOG_WARN("Participants 0x%x is not part of this group!", ssrc);
            continue;
        }

        delete participants_[ssrc]->socket;
        delete participants_[ssrc];
        participants_.erase(ssrc);
    }

    return RTP_OK;
}

rtp_error_t uvgrtp::rtcp::send_bye_packet(std::vector<uint32_t> ssrcs)
{
    if (ssrcs.empty()) {
        LOG_WARN("Source Count in RTCP BYE packet is 0");
    }

    size_t frame_size;
    rtp_error_t ret;
    uint8_t *frame;
    int ptr = 4;

    frame_size  = 4; /* rtcp header */
    frame_size += ssrcs.size() * sizeof(uint32_t);

    frame = new uint8_t[frame_size];
    memset(frame, 0, frame_size);

    frame[0] = (2 << 6) | (0 << 5) | (ssrcs.size() & 0x1f);
    frame[1] = uvgrtp::frame::RTCP_FT_BYE;

    for (auto& ssrc : ssrcs)
        SET_NEXT_FIELD_32(frame, ptr, htonl(ssrc));

    for (auto& p : participants_) {
        if ((ret = p.second->socket->sendto(p.second->address, frame, frame_size, 0)) != RTP_OK) {
            LOG_ERROR("sendto() failed!");
            goto end;
        }

        update_rtcp_bandwidth(frame_size);
    }

end:
    delete[] frame;
    return ret;
}
