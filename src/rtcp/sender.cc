#ifdef _WIN32
#else
#endif

#include "rtcp.hh"

uvg_rtp::frame::rtcp_sender_frame *uvg_rtp::rtcp::get_sender_packet(uint32_t ssrc)
{
    if (participants_.find(ssrc) == participants_.end())
        return nullptr;

    auto frame = participants_[ssrc]->s_frame;
    participants_[ssrc]->s_frame = nullptr;

    return frame;
}

rtp_error_t uvg_rtp::rtcp::install_sender_hook(void (*hook)(uvg_rtp::frame::rtcp_sender_frame *))
{
    if (!hook)
        return RTP_INVALID_VALUE;

    sender_hook_ = hook;
    return RTP_OK;
}

rtp_error_t uvg_rtp::rtcp::handle_sender_report_packet(uint8_t *frame, size_t size)
{
#if 0
    (void)size;

    if (!frame)
        return RTP_INVALID_VALUE;

    frame->sender_ssrc = ntohl(frame->sender_ssrc);

    if (!is_participant(frame->sender_ssrc))
        add_participant(frame->sender_ssrc);

    uint32_t ntp_msw = ntohl(frame->s_info.ntp_msw);
    uint32_t ntp_lsw = ntohl(frame->s_info.ntp_lsw);
    uint32_t lsr     = ((ntp_msw >> 16) & 0xffff) | ((ntp_lsw & 0xffff0000) >> 16);

    participants_[frame->sender_ssrc]->stats.lsr   = lsr;
    participants_[frame->sender_ssrc]->stats.sr_ts = uvg_rtp::clock::hrc::now();

    /* We need to make a copy of the frame because right now frame points to RTCP recv buffer
     * Deallocate previous frame if it exists */
    if (participants_[frame->sender_ssrc]->s_frame != nullptr)
        (void)uvg_rtp::frame::dealloc_frame(participants_[frame->sender_ssrc]->s_frame);

    auto cpy_frame = uvg_rtp::frame::alloc_rtcp_sender_frame(frame->header.count);
    memcpy(cpy_frame, frame, size);

    fprintf(stderr, "Sender report:\n");
    for (int i = 0; i < frame->header.count; ++i) {
        cpy_frame->blocks[i].lost     = ntohl(frame->blocks[i].lost);
        cpy_frame->blocks[i].last_seq = ntohl(frame->blocks[i].last_seq);
        cpy_frame->blocks[i].lsr      = ntohl(frame->blocks[i].lsr);
        cpy_frame->blocks[i].dlsr     = ntohl(frame->blocks[i].dlsr);

        fprintf(stderr, "-------\n");
        fprintf(stderr, "lost:     %d\n", cpy_frame->blocks[i].lost);
        fprintf(stderr, "last_seq: %u\n", cpy_frame->blocks[i].last_seq);
        fprintf(stderr, "last sr:  %u\n", cpy_frame->blocks[i].lsr);
        fprintf(stderr, "dlsr:     %u\n", cpy_frame->blocks[i].dlsr);
        fprintf(stderr, "-------\n");
    }

    if (sender_hook_)
        sender_hook_(cpy_frame);
    else
        participants_[frame->sender_ssrc]->s_frame = cpy_frame;

#endif
    return RTP_OK;
}

rtp_error_t uvg_rtp::rtcp::send_sender_report_packet(uvg_rtp::frame::rtcp_sender_frame *frame)
{
    LOG_INFO("Generating sender report...");

    if (!frame)
        return RTP_INVALID_VALUE;

    rtp_error_t ret = RTP_OK;
    std::vector<uint32_t> ssrcs;
    uint16_t len = frame->header.length;

    /* RTCP header + SSRC */
    frame->header.length = htons(frame->header.length);
    frame->sender_ssrc   = htonl(frame->sender_ssrc);

    /* RTCP Sender Info */
    frame->s_info.ntp_msw  = htonl(frame->s_info.ntp_msw);
    frame->s_info.ntp_lsw  = htonl(frame->s_info.ntp_lsw);
    frame->s_info.rtp_ts   = htonl(frame->s_info.rtp_ts);
    frame->s_info.pkt_cnt  = htonl(frame->s_info.pkt_cnt);
    frame->s_info.byte_cnt = htonl(frame->s_info.byte_cnt);

    /* report block(s) */
    for (size_t i = 0; i < frame->header.count; ++i) {
        ssrcs.push_back(frame->blocks[i].ssrc);

        frame->blocks[i].last_seq = htonl(frame->blocks[i].last_seq);
        frame->blocks[i].jitter   = htonl(frame->blocks[i].jitter);
        frame->blocks[i].ssrc     = htonl(frame->blocks[i].ssrc);
        frame->blocks[i].lost     = htonl(frame->blocks[i].lost);
        frame->blocks[i].dlsr     = htonl(frame->blocks[i].dlsr);
        frame->blocks[i].lsr      = htonl(frame->blocks[i].lsr);
    }

    for (auto& p : participants_) {
        if ((ret = p.second->socket->sendto(p.second->address, (uint8_t *)frame, len, 0)) != RTP_OK) {
            LOG_ERROR("sendto() failed!");
        }

        update_rtcp_bandwidth(len);
    }

    return ret;
}

rtp_error_t uvg_rtp::rtcp::generate_sender_report()
{
    /* No one to generate report for */
    if (num_receivers_ == 0)
        return RTP_NOT_READY;

    uvg_rtp::frame::rtcp_sender_frame *frame;

    if ((frame = uvg_rtp::frame::alloc_rtcp_sender_frame(senders_)) == nullptr) {
        LOG_ERROR("Failed to allocate RTCP Receiver Report frame!");
        return rtp_errno;
    }

    size_t ptr         = 0;
    uint64_t timestamp = uvg_rtp::clock::ntp::now();
    rtp_error_t ret    = RTP_OK;

    frame->header.count    = senders_;
    frame->sender_ssrc     = ssrc_;
    frame->s_info.ntp_msw  = timestamp >> 32;
    frame->s_info.ntp_lsw  = timestamp & 0xffffffff;
    frame->s_info.rtp_ts   = rtp_ts_start_ + (uvg_rtp::clock::ntp::diff(timestamp, clock_start_)) * clock_rate_ / 1000;
    frame->s_info.pkt_cnt  = our_stats.sent_pkts;
    frame->s_info.byte_cnt = our_stats.sent_bytes;

    LOG_DEBUG("Sender Report from 0x%x has %zu blocks", ssrc_, senders_);

    for (auto& participant : participants_) {
        if (participant.second->role == RECEIVER)
            continue;

        frame->blocks[ptr].ssrc = participant.first;

        if (participant.second->stats.dropped_pkts != 0) {
            frame->blocks[ptr].fraction =
                participant.second->stats.received_pkts / participant.second->stats.dropped_pkts;
        }

        frame->blocks[ptr].lost     = participant.second->stats.dropped_pkts;
        frame->blocks[ptr].last_seq = participant.second->stats.max_seq;
        frame->blocks[ptr].jitter   = participant.second->stats.jitter;
        frame->blocks[ptr].lsr      = participant.second->stats.lsr;

        ptr++;
    }

    /* Send sender report only if the session contains other senders */
    if (ptr != 0)
        ret = uvg_rtp::rtcp::send_sender_report_packet(frame);

    (void)uvg_rtp::frame::dealloc_frame(frame);

    return ret;
}
