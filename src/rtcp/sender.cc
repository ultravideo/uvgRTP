#include "rtcp.hh"

#include "../srtp/srtcp.hh"
#include "debug.hh"

uvgrtp::frame::rtcp_sender_report *uvgrtp::rtcp::get_sender_packet(uint32_t ssrc)
{
    if (participants_.find(ssrc) == participants_.end())
        return nullptr;

    auto frame = participants_[ssrc]->s_frame;
    participants_[ssrc]->s_frame = nullptr;

    return frame;
}

rtp_error_t uvgrtp::rtcp::install_sender_hook(void (*hook)(uvgrtp::frame::rtcp_sender_report *))
{
    if (!hook)
        return RTP_INVALID_VALUE;

    sender_hook_ = hook;
    return RTP_OK;
}

rtp_error_t uvgrtp::rtcp::handle_sender_report_packet(uint8_t *packet, size_t size)
{
    if (!packet || !size)
        return RTP_INVALID_VALUE;

    auto srtpi = (*(uint32_t *)&packet[size - UVG_SRTCP_INDEX_LENGTH - UVG_AUTH_TAG_LENGTH]);
    auto frame = new uvgrtp::frame::rtcp_sender_report;
    auto ret   = RTP_OK;

    frame->header.version = (packet[0] >> 6) & 0x3;
    frame->header.padding = (packet[0] >> 5) & 0x1;
    frame->header.count   = packet[0] & 0x1f;
    frame->header.length  = ntohs(*(uint16_t *)&packet[2]);
    frame->ssrc           = ntohl(*(uint32_t *)&packet[4]);

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

    if (!is_participant(frame->ssrc)) {
        LOG_WARN("Sender Report received from an unknown participant");
        add_participant(frame->ssrc);
    }

    /* Deallocate previous frame from the buffer if it exists, it's going to get overwritten */
    if (participants_[frame->ssrc]->s_frame)
        delete participants_[frame->ssrc]->s_frame;

    frame->sender_info.ntp_msw  = ntohl(*(uint32_t *)&packet[ 8]);
    frame->sender_info.ntp_lsw  = ntohl(*(uint32_t *)&packet[12]);
    frame->sender_info.rtp_ts   = ntohl(*(uint32_t *)&packet[16]);
    frame->sender_info.pkt_cnt  = ntohl(*(uint32_t *)&packet[20]);
    frame->sender_info.byte_cnt = ntohl(*(uint32_t *)&packet[24]);

    participants_[frame->ssrc]->stats.sr_ts = uvgrtp::clock::hrc::now();
    participants_[frame->ssrc]->stats.lsr   =
        ((frame->sender_info.ntp_msw >> 16) & 0xffff) |
        ((frame->sender_info.ntp_lsw & 0xffff0000) >> 16);

    for (int i = 0; i < frame->header.count; ++i) {
        uvgrtp::frame::rtcp_report_block report;

        report.ssrc     =  ntohl(*(uint32_t *)&packet[(i * 24) + 28 +  0]);
        report.fraction = (ntohl(*(uint32_t *)&packet[(i * 24) + 28 +  4])) >> 24;
        report.lost     = (ntohl(*(uint32_t *)&packet[(i * 24) + 28 +  4])) & 0xfffffd;
        report.last_seq =  ntohl(*(uint32_t *)&packet[(i * 24) + 28 +  8]);
        report.jitter   =  ntohl(*(uint32_t *)&packet[(i * 24) + 28 + 12]);
        report.lsr      =  ntohl(*(uint32_t *)&packet[(i * 24) + 28 + 16]);
        report.dlsr     =  ntohl(*(uint32_t *)&packet[(i * 24) + 28 + 20]);

        frame->report_blocks.push_back(report);
    }

    if (sender_hook_)
        sender_hook_(frame);
    else
        participants_[frame->ssrc]->s_frame = frame;

    return RTP_OK;
}

rtp_error_t uvgrtp::rtcp::generate_sender_report()
{
    LOG_DEBUG("Generating RTCP Sender Report");

    if (!senders_) {
        LOG_DEBUG("Session does not have any RTP senders!");
        return RTP_NOT_READY;
    }

    uint64_t ntp_ts, rtp_ts;
    size_t frame_size;
    rtp_error_t ret;
    uint8_t *frame;
    int ptr = 8;

    frame_size  = 4;                   /* rtcp header */
    frame_size += 4;                   /* our ssrc */
    frame_size += 20;                  /* sender info */
    frame_size += num_receivers_ * 24; /* report blocks */

    if (flags_ & RCE_SRTP)
        frame_size += UVG_SRTCP_INDEX_LENGTH + UVG_AUTH_TAG_LENGTH;

    frame = new uint8_t[frame_size];
    memset(frame, 0, frame_size);

    frame[0] = (2 << 6) | (0 << 5) | num_receivers_;
    frame[1] = uvgrtp::frame::RTCP_FT_SR;

    *(uint16_t *)&frame[2] = htons((u_short)frame_size);
    *(uint32_t *)&frame[4] = htonl(ssrc_);

    /* Sender information */
    ntp_ts = uvgrtp::clock::ntp::now();
    rtp_ts = rtp_ts_start_ + (uvgrtp::clock::ntp::diff(ntp_ts, clock_start_)) * clock_rate_ / 1000;

    SET_NEXT_FIELD_32(frame, ptr, htonl(ntp_ts >> 32));
    SET_NEXT_FIELD_32(frame, ptr, htonl(ntp_ts & 0xffffffff));
    SET_NEXT_FIELD_32(frame, ptr, htonl((u_long)rtp_ts));
    SET_NEXT_FIELD_32(frame, ptr, htonl(our_stats.sent_pkts));
    SET_NEXT_FIELD_32(frame, ptr, htonl(our_stats.sent_bytes));

    LOG_DEBUG("Sender Report from 0x%x has %zu blocks", ssrc_, num_receivers_);

    for (auto& p : participants_) {
        int dropped  = p.second->stats.dropped_pkts;
        uint8_t frac = dropped ? p.second->stats.received_bytes / dropped : 0;

        SET_NEXT_FIELD_32(frame, ptr, htonl(p.first)); /* ssrc */
        SET_NEXT_FIELD_32(frame, ptr, htonl((frac << 24) | p.second->stats.dropped_pkts));
        SET_NEXT_FIELD_32(frame, ptr, htonl(p.second->stats.max_seq));
        SET_NEXT_FIELD_32(frame, ptr, htonl(p.second->stats.jitter));
        SET_NEXT_FIELD_32(frame, ptr, htonl(p.second->stats.lsr));

        /* calculate delay of last SR only if SR has been received at least once */
        if (p.second->stats.lsr) {
            uint64_t diff = (u_long)uvgrtp::clock::hrc::diff_now(p.second->stats.sr_ts);
            SET_NEXT_FIELD_32(frame, ptr, (uint32_t)htonl((u_long)uvgrtp::clock::ms_to_jiffies(diff)));
        }
        ptr += p.second->stats.lsr ? 0 : 4;
    }

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
            log_platform_error("sendto(2) failed");
            goto end;
        }

        update_rtcp_bandwidth(frame_size);
    }

end:
    delete[] frame;
    return ret;
}
