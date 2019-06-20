#ifdef _WIN32
#include <windows.h>
#else
#include <sys/time.h>
#endif

#include <cstring>
#include <iostream>

#include "debug.hh"
#include "poll.hh"
#include "rtcp.hh"
#include "util.hh"

kvz_rtp::rtcp::rtcp(bool receiver):
    tp_(0), tc_(0), tn_(0), pmembers_(0),
    members_(0), senders_(0), rtcp_bandwidth_(0),
    we_sent_(0), avg_rtcp_pkt_pize_(0), initial_(true),
    active_(false), send_addr_(""), send_port_(0), recv_port_(0),
    num_receivers_(0), receiver_(receiver)
{
    cname_ = "hello"; //generate_cname();

    memset(&sender_stats, 0, sizeof(sender_stats));
}

kvz_rtp::rtcp::~rtcp()
{
}

rtp_error_t kvz_rtp::rtcp::add_participant(std::string dst_addr, int dst_port, int src_port)
{
    if (dst_addr == "" || dst_port == 0 || src_port == 0) {
        LOG_ERROR("Invalid values given (%s, %d, %d), cannot create RTCP instance",
                dst_addr.c_str(), dst_port, src_port);
        return RTP_INVALID_VALUE;
    }

    rtp_error_t ret;
    struct participant *p;

    if ((p = new struct participant) == nullptr)
        return RTP_MEMORY_ERROR;

    if ((p->socket = new kvz_rtp::socket()) == nullptr)
        return RTP_MEMORY_ERROR;

    if ((ret = p->socket->init(AF_INET, SOCK_DGRAM, 0)) != RTP_OK)
        return ret;

    int enable = 1;

    if ((ret = p->socket->setsockopt(SOL_SOCKET, SO_REUSEADDR, (const char *)&enable, sizeof(int))) != RTP_OK)
        return ret;

    /* Set read timeout (5s for now) 
     *
     * This means that the socket is listened for 5s at a time and after the timeout, 
     * Send Report is sent to all participants */
    struct timeval tv = {
        .tv_sec  = 3,
        .tv_usec = 0
    };

    if ((ret = p->socket->setsockopt(SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv))) != RTP_OK)
        return ret;

    LOG_WARN("Binding to port %d (source port)", src_port);

    if ((ret = p->socket->bind(AF_INET, INADDR_ANY, src_port)) != RTP_OK)
        return ret;

    p->address = p->socket->create_sockaddr(AF_INET, dst_addr, dst_port);

    initial_peers_.push_back(p);
    sockets_.push_back(*p->socket);

    return RTP_OK;
}

void kvz_rtp::rtcp::set_sender_ssrc(sockaddr_in& addr, uint32_t ssrc)
{
    if (participants_.find(ssrc) != participants_.end()) {
        LOG_ERROR("SSRC clash detected, must be resolved!");
        return;
    }

    /* TODO: this is not correct, find the sender from initial_peers_ (TODO: how??) */
    auto peer = initial_peers_.back();
    participants_[ssrc] = peer;
    initial_peers_.pop_back();
}

rtp_error_t kvz_rtp::rtcp::start()
{
    if (sockets_.empty()) {
        LOG_ERROR("Cannot start RTCP Runner because no connections have been initialized");
        return RTP_INVALID_VALUE;
    }

    active_ = true;

    if ((runner_ = new std::thread(rtcp_runner, this)) == nullptr) {
        active_ = false;
        LOG_ERROR("Failed to create RTCP thread!");
        return RTP_MEMORY_ERROR;
    }
    runner_->detach();

    return RTP_OK;
}

rtp_error_t kvz_rtp::rtcp::terminate()
{
    /* when the member count is less than 50,
     * we can just send the BYE message and destroy the session */
    if (members_ < 50) {
        active_ = false;
        goto end;
    }

    tp_       = tc_;
    members_  = 1;
    pmembers_ = 1;
    initial_  = true;
    we_sent_  = false;
    senders_  = 0;
    active_   = false;

end:
    /* free all receiver statistic structs */
    for (auto& participant : participants_) {
        delete participant.second->socket;
        delete participant.second;
    }

    return RTP_OK;
}

bool kvz_rtp::rtcp::active() const
{
    return active_;
}

bool kvz_rtp::rtcp::receiver() const
{
    return receiver_;
}

std::vector<kvz_rtp::socket>& kvz_rtp::rtcp::get_sockets()
{
    return sockets_;
}

bool kvz_rtp::rtcp::is_valid_sender(uint32_t ssrc)
{
    return participants_.find(ssrc) != participants_.end();
}

void kvz_rtp::rtcp::sender_inc_sent_bytes(size_t n)
{
    sender_stats.sent_bytes += n;
}

void kvz_rtp::rtcp::sender_inc_sent_pkts(size_t n)
{
    sender_stats.sent_pkts += n;
}

void kvz_rtp::rtcp::sender_update_stats(kvz_rtp::frame::rtp_frame *frame)
{
    if (!frame)
        return;

    sender_stats.sent_pkts   += 1;
    sender_stats.sent_bytes  += frame->payload_len;
    sender_stats.highest_seq  = frame->seq;
}

void kvz_rtp::rtcp::receiver_inc_sent_bytes(uint32_t sender_ssrc, size_t n)
{
    if (is_valid_sender(sender_ssrc)) {
        participants_[sender_ssrc]->stats.sent_bytes += n;
        return;
    }

    LOG_WARN("Got RTP packet from unknown source: 0x%x", sender_ssrc);
}

void kvz_rtp::rtcp::receiver_inc_sent_pkts(uint32_t sender_ssrc, size_t n)
{
    if (is_valid_sender(sender_ssrc)) {
        participants_[sender_ssrc]->stats.sent_pkts += n;
        return;
    }

    LOG_WARN("Got RTP packet from unknown source: 0x%x", sender_ssrc);
}

void kvz_rtp::rtcp::receiver_update_stats(kvz_rtp::frame::rtp_frame *frame)
{
    if (!frame)
        return;

    if (!is_valid_sender(frame->ssrc)) {
        LOG_WARN("Got RTP packet from unknown source: 0x%x", frame->ssrc);
        return;
    }

    participants_[frame->ssrc]->stats.sent_pkts   += 1;
    participants_[frame->ssrc]->stats.sent_bytes  += frame->payload_len;
    participants_[frame->ssrc]->stats.highest_seq  = frame->seq;
}

rtp_error_t kvz_rtp::rtcp::send_sender_report_packet(kvz_rtp::frame::rtcp_sender_frame *frame)
{
    LOG_INFO("Generating sender report...");

    if (!frame)
        return RTP_INVALID_VALUE;

    rtp_error_t ret;
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

    for (uint32_t& ssrc : ssrcs) {
        if (!is_valid_sender(ssrc)) {
            LOG_WARN("Unknown participant 0x%x", ssrc);
            continue;
        }

        auto p = participants_[ssrc];
        if ((ret = p->socket->sendto(p->address, (uint8_t *)frame, len, 0)) != RTP_OK) {
            LOG_ERROR("sendto() failed!");
        }
    }
}

rtp_error_t kvz_rtp::rtcp::send_receiver_report_packet(kvz_rtp::frame::rtcp_receiver_frame *frame)
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

        if ((ret = p->socket->sendto(p->address, (uint8_t *)frame, len, 0)) != RTP_OK) {
            LOG_ERROR("sendto() failed!");
            return ret;
        }
    }

    return ret;
}

rtp_error_t kvz_rtp::rtcp::send_bye_packet(kvz_rtp::frame::rtcp_bye_frame *frame)
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
    }
}

rtp_error_t kvz_rtp::rtcp::send_sdes_packet(kvz_rtp::frame::rtcp_sdes_frame *frame)
{
    if (!frame)
        return RTP_INVALID_VALUE;

    if (frame->header.count == 0) {
        LOG_WARN("");
    }

    uint16_t len = frame->header.length;

    /* rtcp header + ssrc */
    frame->header.length = htons(frame->header.length);
    frame->sender_ssrc = htonl(frame->sender_ssrc);

    for (size_t i = 0; i < frame->header.count; ++i) {
        frame->items[i].length = htons(frame->items[i].length);
    }

    rtp_error_t ret;

    for (auto& participant : participants_) {
        auto p = participant.second;

        if ((ret = p->socket->sendto(p->address, (uint8_t *)frame, len, 0)) != RTP_OK) {
            LOG_ERROR("sendto() failed!");
            return ret;
        }
    }
}

rtp_error_t kvz_rtp::rtcp::send_app_packet(kvz_rtp::frame::rtcp_app_frame *frame)
{
    if (!frame)
        return RTP_INVALID_VALUE;

    uint16_t len  = frame->length;
    uint32_t ssrc = frame->ssrc;

    frame->length = htons(frame->length);
    frame->ssrc   = htonl(frame->ssrc);

    if (is_valid_sender(ssrc))
        return participants_[ssrc]->socket->sendto((uint8_t *)frame, len, 0, NULL);

    LOG_ERROR("Unknown participant 0x%x", ssrc);
    return RTP_INVALID_VALUE;
}

rtp_error_t kvz_rtp::rtcp::generate_sender_report()
{
    return RTP_OK;

    LOG_INFO("Generating sender report...");

    kvz_rtp::frame::rtcp_sender_frame *frame;

    if ((frame = kvz_rtp::frame::alloc_rtcp_sender_frame(num_receivers_)) == nullptr) {
        LOG_ERROR("Failed to allocate RTCP Receiver Report frame!");
        return rtp_errno;
    }

    size_t ptr         = 0;
    uint64_t timestamp = tv_to_ntp();

    frame->s_info.ntp_msw  = timestamp >> 32;
    frame->s_info.ntp_lsw  = timestamp & 0xffffffff;
    frame->s_info.rtp_ts   = 3; /* TODO: what timestamp is this? */
    frame->s_info.pkt_cnt  = sender_stats.sent_pkts;
    frame->s_info.byte_cnt = sender_stats.sent_bytes;

    /* TODO: is this correct?? 
     * what information should we sent here?? */

    for (auto& participant : participants_) {
        frame->blocks[ptr].ssrc = participant.first;

        if (participant.second->stats.dropped_pkts != 0) {
            frame->blocks[ptr].fraction =
                participant.second->stats.sent_pkts / participant.second->stats.dropped_pkts;
        }

        frame->blocks[ptr].lost     = participant.second->stats.dropped_pkts;
        frame->blocks[ptr].last_seq = participant.second->stats.highest_seq;
        frame->blocks[ptr].jitter   = participant.second->stats.jitter;
        frame->blocks[ptr].dlsr     = participant.second->stats.lsr_delay;
        frame->blocks[ptr].lsr      = participant.second->stats.lsr_ts;

        ptr++;
    }

    return send_sender_report_packet(frame);
}

rtp_error_t kvz_rtp::rtcp::generate_receiver_report()
{
    LOG_INFO("Generating receiver report...");

    kvz_rtp::frame::rtcp_receiver_frame *frame;

    if ((frame = kvz_rtp::frame::alloc_rtcp_receiver_frame(num_receivers_)) == nullptr) {
        LOG_ERROR("Failed to allocate RTCP Receiver Report frame!");
        return rtp_errno;
    }

    size_t ptr = 0;

    /* TODO: is this correct?? 
     * what information should we sent here?? */

    for (auto& participant : participants_) {
        frame->blocks[ptr].ssrc = participant.first;

        if (participant.second->stats.dropped_pkts != 0) {
            frame->blocks[ptr].fraction =
                participant.second->stats.sent_pkts / participant.second->stats.dropped_pkts;
        }

        frame->blocks[ptr].lost     = participant.second->stats.dropped_pkts;
        frame->blocks[ptr].last_seq = participant.second->stats.highest_seq;
        frame->blocks[ptr].jitter   = participant.second->stats.jitter;
        frame->blocks[ptr].dlsr     = participant.second->stats.lsr_delay;
        frame->blocks[ptr].lsr      = participant.second->stats.lsr_ts;

        ptr++;
    }

    return send_receiver_report_packet(frame);
}

rtp_error_t kvz_rtp::rtcp::generate_report()
{
    if (receiver_)
        return generate_receiver_report();
    return generate_sender_report();
}

rtp_error_t kvz_rtp::rtcp::handle_sender_report_packet(kvz_rtp::frame::rtcp_sender_frame *frame)
{
    if (!frame)
        return RTP_INVALID_VALUE;

    if (frame->header.count == 0) {
        LOG_ERROR("Sender Report cannot have 0 report blocks!");
        return RTP_INVALID_VALUE;
    }

    /* TODO: 6.4.4 Analyzing Sender and Receiver Reports */

    return RTP_OK;
}

rtp_error_t kvz_rtp::rtcp::handle_receiver_report_packet(kvz_rtp::frame::rtcp_receiver_frame *frame)
{
    if (!frame)
        return RTP_INVALID_VALUE;

    if (frame->header.count == 0) {
        LOG_ERROR("Receiver Report cannot have 0 report blocks!");
        return RTP_INVALID_VALUE;
    }

    /* TODO: 6.4.4 Analyzing Sender and Receiver Reports */

    return RTP_OK;
}

rtp_error_t kvz_rtp::rtcp::handle_sdes_packet(kvz_rtp::frame::rtcp_sdes_frame *frame)
{
    if (!frame)
        return RTP_INVALID_VALUE;

    if (frame->header.count == 0) {
        LOG_ERROR("SDES packet cannot contain 0 fields!");
        return RTP_INVALID_VALUE;
    }

    /* TODO: What to do with SDES packet */

    return RTP_OK;
}

rtp_error_t kvz_rtp::rtcp::handle_bye_packet(kvz_rtp::frame::rtcp_bye_frame *frame)
{
    if (!frame)
        return RTP_INVALID_VALUE;

    /* TODO: remove this participant from the map */
    /* TODO: implement participant map + support for multiple participants */

    return RTP_OK;
}

rtp_error_t kvz_rtp::rtcp::handle_app_packet(kvz_rtp::frame::rtcp_app_frame *frame)
{
    if (!frame)
        return RTP_INVALID_VALUE;

    /* TODO: What to do with APP packet */

    return RTP_OK;
}

rtp_error_t kvz_rtp::rtcp::handle_incoming_packet(uint8_t *buffer, size_t size)
{
    (void)size;

    kvz_rtp::frame::rtcp_header *header = (kvz_rtp::frame::rtcp_header *)buffer;

    if (header->version != 2) {
        LOG_ERROR("Invalid header version (%u)", header->version);
        return RTP_INVALID_VALUE;
    }

    if (header->padding != 0) {
        LOG_ERROR("Cannot handle padded packets!");
        return RTP_INVALID_VALUE;
    }
    
    if (header->pkt_type > kvz_rtp::frame::FRAME_TYPE_BYE ||
        header->pkt_type < kvz_rtp::frame::FRAME_TYPE_SR) {
        LOG_ERROR("Invalid packet type (%u)!", header->pkt_type);
        return RTP_INVALID_VALUE;
    }

    rtp_error_t ret = RTP_INVALID_VALUE;

    switch (header->pkt_type) {
        case kvz_rtp::frame::FRAME_TYPE_SR:
            ret = handle_sender_report_packet((kvz_rtp::frame::rtcp_sender_frame *)buffer);
            break;

        case kvz_rtp::frame::FRAME_TYPE_RR:
            ret = handle_receiver_report_packet((kvz_rtp::frame::rtcp_receiver_frame *)buffer);
            break;

        case kvz_rtp::frame::FRAME_TYPE_SDES:
            ret = handle_sdes_packet((kvz_rtp::frame::rtcp_sdes_frame *)buffer);
            break;

        case kvz_rtp::frame::FRAME_TYPE_BYE:
            ret = handle_bye_packet((kvz_rtp::frame::rtcp_bye_frame *)buffer);
            break;

        case kvz_rtp::frame::FRAME_TYPE_APP:
            ret = handle_app_packet((kvz_rtp::frame::rtcp_app_frame *)buffer);
            break;
    }

    return ret;
}

void kvz_rtp::rtcp::rtcp_runner(kvz_rtp::rtcp *rtcp)
{
    LOG_INFO("RTCP instance created!");

    int nread;
    uint8_t buffer[MAX_PACKET];

    while (rtcp->active()) {
        rtp_error_t ret = kvz_rtp::poll::poll(rtcp->get_sockets(), buffer, MAX_PACKET, 1500, &nread);

        if (ret == RTP_OK && nread > 0) {
            (void)rtcp->handle_incoming_packet(buffer, (size_t)nread);
        } else if (ret == RTP_INTERRUPTED) {
            /* do nothing */
        } else {
            LOG_ERROR("recvfrom failed, %d", ret);
        }

        if ((ret = rtcp->generate_report()) != RTP_OK) {
            LOG_ERROR("Failed to send RTCP status report!");
        }
    }
}

uint64_t kvz_rtp::rtcp::tv_to_ntp()
{
    static const uint64_t EPOCH = 2208988800ULL;
    static const uint64_t NTP_SCALE_FRAC = 4294967296ULL;

    static struct timeval tv;
    gettimeofday(&tv, NULL);

    uint64_t tv_ntp, tv_usecs;

    tv_ntp = tv.tv_sec + EPOCH;
    tv_usecs = (NTP_SCALE_FRAC * tv.tv_usec) / 1000000UL;

    return (tv_ntp << 32) | tv_usecs;
}
