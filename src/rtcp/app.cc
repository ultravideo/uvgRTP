#include "rtcp.hh"

#include "../srtp/srtcp.hh"
#include "debug.hh"

uvgrtp::frame::rtcp_app_packet *uvgrtp::rtcp::get_app_packet(uint32_t ssrc)
{
    if (participants_.find(ssrc) == participants_.end())
        return nullptr;

    auto frame = participants_[ssrc]->app_frame;
    participants_[ssrc]->app_frame = nullptr;

    return frame;
}

rtp_error_t uvgrtp::rtcp::install_app_hook(void (*hook)(uvgrtp::frame::rtcp_app_packet *))
{
    if (!hook)
        return RTP_INVALID_VALUE;

    app_hook_ = hook;
    return RTP_OK;
}

rtp_error_t uvgrtp::rtcp::handle_app_packet(uint8_t *packet, size_t size)
{
    if (!packet || !size)
        return RTP_INVALID_VALUE;

    auto frame = new uvgrtp::frame::rtcp_app_packet;
    read_rtcp_header(packet, frame->header, true);
    frame->ssrc               = ntohl(*(uint32_t *)&packet[4]);

    auto ret = RTP_OK;
    if (srtcp_ && (ret = srtcp_->handle_rtcp_decryption(flags_, frame->ssrc, packet, size)) != RTP_OK)
        return ret;

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

rtp_error_t uvgrtp::rtcp::send_app_packet(char *name, uint8_t subtype, 
    size_t payload_len, uint8_t *payload)
{
    size_t frame_size;
    rtp_error_t ret = RTP_OK;
    uint8_t *frame;

    frame_size  = 4; /* rtcp header */
    frame_size += 4; /* our ssrc */
    frame_size += 4; /* name */
    frame_size += payload_len;

    construct_rtcp_header(frame_size, frame, (subtype & 0x1f), uvgrtp::frame::RTCP_FT_APP, true);

    memcpy(&frame[ 8],    name,           4);
    memcpy(&frame[12], payload, payload_len);

    if (srtcp_ && (ret = srtcp_->handle_rtcp_encryption(flags_, rtcp_pkt_sent_count_, ssrc_, frame, frame_size)) != RTP_OK)
        return ret;

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
