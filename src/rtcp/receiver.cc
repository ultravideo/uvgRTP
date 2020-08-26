#ifdef _WIN32
#else
#endif

#include "rtcp.hh"

#define SET_NEXT_FIELD_32(a, p, v) do { *(uint32_t *)&(a)[p] = (v); ptr += 4; } while (0)

uvg_rtp::frame::rtcp_receiver_frame *uvg_rtp::rtcp::get_receiver_packet(uint32_t ssrc)
{
    if (participants_.find(ssrc) == participants_.end())
        return nullptr;

    auto frame = participants_[ssrc]->r_frame;
    participants_[ssrc]->r_frame = nullptr;

    return frame;
}

rtp_error_t uvg_rtp::rtcp::install_receiver_hook(void (*hook)(uvg_rtp::frame::rtcp_receiver_frame *))
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

    frame->header.length = ntohs(frame->header.length);
    frame->sender_ssrc   = ntohl(frame->sender_ssrc);

    /* Receiver Reports are sent from participant that don't send RTP packets
     * This means that the sender of this report is not in the participants_ map
     * but rather in the initial_participants_ vector
     *
     * Check if that's the case and if so, move the entry from initial_participants_ to participants_ */
    if (!is_participant(frame->sender_ssrc)) {
        /* TODO: this is not correct way to do it! fix before multicast */
        add_participant(frame->sender_ssrc);
    }

    if (frame->header.count == 0) {
        LOG_ERROR("Receiver Report cannot have 0 report blocks!");
        return RTP_INVALID_VALUE;
    }

    /* We need to make a copy of the frame because right now frame points to RTCP recv buffer
     * Deallocate previous frame if it exists */
    if (participants_[frame->sender_ssrc]->r_frame != nullptr)
        (void)uvg_rtp::frame::dealloc_frame(participants_[frame->sender_ssrc]->r_frame);

    auto cpy_frame = uvg_rtp::frame::alloc_rtcp_receiver_frame(frame->header.count);
    memcpy(cpy_frame, frame, size);

    fprintf(stderr, "Receiver report:\n");
    for (int i = 0; i < frame->header.count; ++i) {
        cpy_frame->blocks[i].lost     = ntohl(frame->blocks[i].lost);
        cpy_frame->blocks[i].last_seq = ntohl(frame->blocks[i].last_seq);
        cpy_frame->blocks[i].jitter   = ntohl(frame->blocks[i].jitter);
        cpy_frame->blocks[i].lsr      = ntohl(frame->blocks[i].lsr);
        cpy_frame->blocks[i].dlsr     = ntohl(frame->blocks[i].dlsr);

        fprintf(stderr, "-------\n");
        fprintf(stderr, "lost:     %d\n", cpy_frame->blocks[i].lost);
        fprintf(stderr, "last_seq: %u\n", cpy_frame->blocks[i].last_seq);
        fprintf(stderr, "jitter:   %u\n", cpy_frame->blocks[i].jitter);
        fprintf(stderr, "last sr:  %u\n", cpy_frame->blocks[i].lsr);
        fprintf(stderr, "dlsr:     %u\n", cpy_frame->blocks[i].dlsr);
        fprintf(stderr, "-------\n");
    }

    if (receiver_hook_)
        receiver_hook_(cpy_frame);
    else
        participants_[frame->sender_ssrc]->r_frame = cpy_frame;

    return RTP_OK;
}

rtp_error_t uvg_rtp::rtcp::generate_receiver_report()
{
    if (!num_receivers_ && !senders_) {
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
