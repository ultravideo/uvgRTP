#ifdef _WIN32
#else
#endif

#include "rtcp.hh"

uvg_rtp::frame::rtcp_app_packet *uvg_rtp::rtcp::get_app_packet(uint32_t ssrc)
{
    if (participants_.find(ssrc) == participants_.end())
        return nullptr;

    auto frame = participants_[ssrc]->app_frame;
    participants_[ssrc]->app_frame = nullptr;

    return frame;
}

rtp_error_t uvg_rtp::rtcp::install_app_hook(void (*hook)(uvg_rtp::frame::rtcp_app_packet *))
{
    if (!hook)
        return RTP_INVALID_VALUE;

    app_hook_ = hook;
    return RTP_OK;
}

rtp_error_t uvg_rtp::rtcp::handle_app_packet(uint8_t *packet, size_t size)
{
    if (!packet || !size)
        return RTP_INVALID_VALUE;

    auto frame = new uvg_rtp::frame::rtcp_app_packet;

    frame->header.version     = (packet[0] >> 6) & 0x3;
    frame->header.padding     = (packet[0] >> 5) & 0x1;
    frame->header.pkt_subtype = packet[0] & 0x1f;
    frame->header.length      = ntohs(*(uint16_t *)&packet[2]);
    frame->ssrc               = ntohl(*(uint32_t *)&packet[4]);

    /* Deallocate previous frame from the buffer if it exists, it's going to get overwritten */
    if (participants_[frame->ssrc]->app_frame) {
        delete[] participants_[frame->ssrc]->app_frame->payload;
        delete   participants_[frame->ssrc]->app_frame;
    }

    memcpy(frame->name,    &packet[ 8],                         4);
    memcpy(frame->payload, &packet[12], frame->header.length - 12);

    if (app_hook_)
        app_hook_(frame);
    else
        participants_[frame->ssrc]->app_frame = frame;

    return RTP_OK;
}

rtp_error_t uvg_rtp::rtcp::send_app_packet(char *name, uint8_t subtype, size_t payload_len, uint8_t *payload)
{
    size_t frame_size;
    rtp_error_t ret;
    uint8_t *frame;

    frame_size  = 4; /* rtcp header */
    frame_size += 4; /* our ssrc */
    frame_size += 4; /* name */
    frame_size += payload_len;

    if (!(frame = new uint8_t[frame_size])) {
        LOG_ERROR("Failed to allocate space for RTCP Receiver Report");
        return RTP_MEMORY_ERROR;
    }
    memset(frame, 0, frame_size);

    frame[0] = (2 << 6) | (0 << 5) | (subtype & 0x1f);
    frame[1] = uvg_rtp::frame::RTCP_FT_APP;

    *(uint16_t *)&frame[2] = htons(frame_size);
    *(uint32_t *)&frame[4] = htonl(ssrc_);

    memcpy(&frame[ 8],    name,           4);
    memcpy(&frame[12], payload, payload_len);

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
