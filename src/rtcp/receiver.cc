#ifdef _WIN32
#else
#endif

#include "rtcp.hh"

uvg_rtp::frame::rtcp_receiver_report *uvg_rtp::rtcp::get_receiver_packet(uint32_t ssrc)
{
    if (participants_.find(ssrc) == participants_.end())
        return nullptr;

    auto frame = participants_[ssrc]->r_frame;
    participants_[ssrc]->r_frame = nullptr;

    return frame;
}

rtp_error_t uvg_rtp::rtcp::install_receiver_hook(void (*hook)(uvg_rtp::frame::rtcp_receiver_report *))
{
    if (!hook)
        return RTP_INVALID_VALUE;

    receiver_hook_ = hook;
    return RTP_OK;
}

rtp_error_t uvg_rtp::rtcp::handle_receiver_report_packet(uint8_t *packet, size_t size)
{
    if (!packet || !size)
        return RTP_INVALID_VALUE;

    auto srtpi = (*(uint32_t *)&packet[size - SRTCP_INDEX_LENGTH - AUTH_TAG_LENGTH]);
    auto frame = new uvg_rtp::frame::rtcp_receiver_report;
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

    /* Receiver Reports are sent from participant that don't send RTP packets
     * This means that the sender of this report is not in the participants_ map
     * but rather in the initial_participants_ vector
     *
     * Check if that's the case and if so, move the entry from initial_participants_ to participants_ */
    if (!is_participant(frame->ssrc)) {
        LOG_WARN("Got a Receiver Report from an unknown participant");
        add_participant(frame->ssrc);
    }

    if (!frame->header.count) {
        LOG_ERROR("Receiver Report cannot have 0 report blocks!");
        return RTP_INVALID_VALUE;
    }

    /* Deallocate previous frame from the buffer if it exists, it's going to get overwritten */
    if (participants_[frame->ssrc]->r_frame)
        delete participants_[frame->ssrc]->r_frame;

    for (int i = 0; i < frame->header.count; ++i) {
        uvg_rtp::frame::rtcp_report_block report;

        report.ssrc     = ntohl(*(uint32_t *)&packet[(i * 24) + 8 + 0]);
        report.lost     = ntohl(*(uint32_t *)&packet[(i * 24) + 8 + 4]);
        report.last_seq = ntohl(*(uint32_t *)&packet[(i * 24) + 8 + 8]);
        report.jitter   = ntohl(*(uint32_t *)&packet[(i * 24) + 8 + 12]);
        report.lsr      = ntohl(*(uint32_t *)&packet[(i * 24) + 8 + 16]);
        report.dlsr     = ntohl(*(uint32_t *)&packet[(i * 24) + 8 + 20]);

        frame->report_blocks.push_back(report);
    }

    if (receiver_hook_)
        receiver_hook_(frame);
    else
        participants_[frame->ssrc]->r_frame = frame;

    return RTP_OK;
}

rtp_error_t uvg_rtp::rtcp::generate_receiver_report()
{
    if (!senders_) {
        LOG_WARN("Session doesn't have any participants!");
        return RTP_NOT_READY;
    }

    size_t frame_size;
    rtp_error_t ret;
    uint8_t *frame;
    int ptr = 8;

    frame_size  = 4;                   /* rtcp header */
    frame_size += 4;                   /* our ssrc */
    frame_size += num_receivers_ * 24; /* report blocks */

    if (flags_ & RCE_SRTP)
        frame_size += SRTCP_INDEX_LENGTH + AUTH_TAG_LENGTH;

    if (!(frame = new uint8_t[frame_size])) {
        LOG_ERROR("Failed to allocate space for RTCP Receiver Report");
        return RTP_MEMORY_ERROR;
    }
    memset(frame, 0, frame_size);

    frame[0] = (2 << 6) | (0 << 5) | num_receivers_;
    frame[1] = uvg_rtp::frame::RTCP_FT_RR;

    *(uint16_t *)&frame[2] = htons(frame_size);
    *(uint32_t *)&frame[4] = htonl(ssrc_);

    LOG_DEBUG("Receiver Report from 0x%x has %zu blocks", ssrc_, num_receivers_);

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
            uint64_t diff = uvg_rtp::clock::hrc::diff_now(p.second->stats.sr_ts);
            SET_NEXT_FIELD_32(frame, ptr, htonl(uvg_rtp::clock::ms_to_jiffies(diff)));
        }
        ptr += p.second->stats.lsr ? 0 : 4;
    }

    /* Encrypt the packet if NULL cipher has not been enabled,
     * calculate authentication tag for the packet and add SRTCP index at the end */
    if (flags_ & RCE_SRTP) {
        if (!(RCE_SRTP & RCE_SRTP_NULL_CIPHER)) {
            srtcp_->encrypt(ssrc_, rtcp_pkt_sent_count_, &frame[8], frame_size - 8 - SRTCP_INDEX_LENGTH - AUTH_TAG_LENGTH);
            SET_FIELD_32(frame, frame_size - SRTCP_INDEX_LENGTH - AUTH_TAG_LENGTH, (1 << 31) | rtcp_pkt_sent_count_);
        } else  {
            SET_FIELD_32(frame, frame_size - SRTCP_INDEX_LENGTH - AUTH_TAG_LENGTH, (0 << 31) | rtcp_pkt_sent_count_);
        }
        srtcp_->add_auth_tag(frame, frame_size);
    }

    for (auto& p : participants_) {
        if ((ret = p.second->socket->sendto(p.second->address, (uint8_t *)frame, frame_size, 0)) != RTP_OK) {
            LOG_ERROR("sendto() failed!");
            delete[] frame;
            return ret;
        }

        update_rtcp_bandwidth(frame_size);
    }

    delete[] frame;
    return RTP_OK;
}
