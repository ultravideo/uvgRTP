#ifdef _WIN32
#else
#endif

#include "rtcp.hh"

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

rtp_error_t uvg_rtp::rtcp::handle_receiver_report_packet(uvg_rtp::frame::rtcp_receiver_frame *frame, size_t size)
{
    (void)size;

    if (!frame)
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

rtp_error_t uvg_rtp::rtcp::send_receiver_report_packet(uvg_rtp::frame::rtcp_receiver_frame *frame)
{
    if (!frame)
        return RTP_INVALID_VALUE;

    rtp_error_t ret;
    uint16_t len = frame->header.length;

    /* rtcp header + ssrc */
    frame->header.length = htons(frame->header.length);
    frame->sender_ssrc   = htonl(frame->sender_ssrc);

    /* report block(s) */
    for (size_t i = 0; i < frame->header.count; ++i) {
        frame->blocks[i].last_seq = htonl(frame->blocks[i].last_seq);
        frame->blocks[i].jitter   = htonl(frame->blocks[i].jitter);
        frame->blocks[i].ssrc     = htonl(frame->blocks[i].ssrc);
        frame->blocks[i].lost     = htonl(frame->blocks[i].lost);
        frame->blocks[i].dlsr     = htonl(frame->blocks[i].dlsr);
        frame->blocks[i].lsr      = htonl(frame->blocks[i].lsr);
    }

    for (auto& participant : participants_) {
        auto p = participant.second;

        /* TODO: bypass socket object */
        if ((ret = p->socket->sendto(p->address, (uint8_t *)frame, len, 0)) != RTP_OK) {
            LOG_ERROR("sendto() failed!");
            return ret;
        }

        update_rtcp_bandwidth(len);
    }

    return ret;
}

rtp_error_t uvg_rtp::rtcp::generate_receiver_report()
{
    /* It is possible that haven't yet received an RTP packet from remote */
    if (num_receivers_ == 0) {
        LOG_WARN("cannot send receiver report yet, haven't received anything");
        return RTP_NOT_READY;
    }

    size_t ptr = 0;
    rtp_error_t ret;
    uvg_rtp::frame::rtcp_receiver_frame *frame;

    if ((frame = uvg_rtp::frame::alloc_rtcp_receiver_frame(num_receivers_)) == nullptr) {
        LOG_ERROR("Failed to allocate RTCP Receiver Report frame!");
        return rtp_errno;
    }

    frame->header.count = num_receivers_;
    frame->sender_ssrc  = ssrc_;

    LOG_INFO("Receiver Report from 0x%x has %zu blocks", ssrc_, num_receivers_);

    for (auto& participant : participants_) {
        frame->blocks[ptr].ssrc = participant.first;

        if (participant.second->stats.dropped_pkts != 0) {
            frame->blocks[ptr].fraction =
                participant.second->stats.received_bytes / participant.second->stats.dropped_pkts;
        }

        frame->blocks[ptr].lost     = participant.second->stats.dropped_pkts;
        frame->blocks[ptr].last_seq = participant.second->stats.max_seq;
        frame->blocks[ptr].jitter   = participant.second->stats.jitter;
        frame->blocks[ptr].lsr      = participant.second->stats.lsr;

        /* calculate delay of last SR only if SR has been received at least once */
        if (frame->blocks[ptr].lsr != 0) {
            uint64_t diff = uvg_rtp::clock::hrc::diff_now(participant.second->stats.sr_ts);
            frame->blocks[ptr].dlsr = uvg_rtp::clock::ms_to_jiffies(diff);
        }

        ptr++;
    }

    ret = uvg_rtp::rtcp::send_receiver_report_packet(frame);
    (void)uvg_rtp::frame::dealloc_frame(frame);

    return ret;
}
