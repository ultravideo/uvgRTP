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

    auto srtpi = (*(uint32_t *)&packet[size - UVG_SRTCP_INDEX_LENGTH - UVG_AUTH_TAG_LENGTH]);
    auto frame = new uvgrtp::frame::rtcp_app_packet;
    auto ret   = RTP_OK;

    frame->header.version     = (packet[0] >> 6) & 0x3;
    frame->header.padding     = (packet[0] >> 5) & 0x1;
    frame->header.pkt_subtype = packet[0] & 0x1f;
    frame->header.length      = ntohs(*(uint16_t *)&packet[2]);
    frame->ssrc               = ntohl(*(uint32_t *)&packet[4]);

    if (flags_ & RCE_SRTP) {
        if ((ret = srtcp_->verify_auth_tag(packet, size)) != RTP_OK) {
            LOG_ERROR("Failed to verify RTCP authentication tag!");
            return RTP_AUTH_TAG_MISMATCH;
        }

        if (((srtpi >> 31) & 0x1) && !(flags_ & RCE_SRTP_NULL_CIPHER)) {
            if (srtcp_->decrypt(frame->ssrc, srtpi & 0x7fffffff, packet, size) != RTP_OK) {
                LOG_ERROR("Failed to decrypt RTCP Sender Report");
                return ret;
            }
        }
    }

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

rtp_error_t uvgrtp::rtcp::send_app_packet(char *name, uint8_t subtype, size_t payload_len, uint8_t *payload)
{
    size_t frame_size;
    rtp_error_t ret = RTP_OK;
    uint8_t *frame;

    frame_size  = 4; /* rtcp header */
    frame_size += 4; /* our ssrc */
    frame_size += 4; /* name */
    frame_size += payload_len;

    frame = new uint8_t[frame_size];
    memset(frame, 0, frame_size);

    frame[0] = (2 << 6) | (0 << 5) | (subtype & 0x1f);
    frame[1] = (uint8_t)uvgrtp::frame::RTCP_FT_APP;

    *(uint16_t *)&frame[2] = htons((u_short)frame_size);
    *(uint32_t *)&frame[4] = htonl(ssrc_);

    memcpy(&frame[ 8],    name,           4);
    memcpy(&frame[12], payload, payload_len);

    /* Encrypt the packet if NULL cipher has not been enabled,
     * calculate authentication tag for the packet and add SRTCP index at the end */
    if (flags_ & RCE_SRTP) {
        if (!(RCE_SRTP & RCE_SRTP_NULL_CIPHER)) {
            srtcp_->encrypt(ssrc_, rtcp_pkt_sent_count_, &frame[8], frame_size - 8 - UVG_SRTCP_INDEX_LENGTH - UVG_AUTH_TAG_LENGTH);
            SET_FIELD_32(frame, frame_size - UVG_SRTCP_INDEX_LENGTH - UVG_AUTH_TAG_LENGTH, (1 << 31) | rtcp_pkt_sent_count_);
        } else  {
            SET_FIELD_32(frame, frame_size - UVG_SRTCP_INDEX_LENGTH - UVG_AUTH_TAG_LENGTH, (0 << 31) | rtcp_pkt_sent_count_);
        }
        srtcp_->add_auth_tag(frame, frame_size);
    }

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
