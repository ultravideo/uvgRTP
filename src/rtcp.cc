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

/* TODO: Find the actual used sizes somehow? */
#define UDP_HEADER_SIZE  8
#define IP_HEADER_SIZE  20

kvz_rtp::rtcp::rtcp(uint32_t ssrc, bool receiver):
    receiver_(receiver),
    tp_(0), tc_(0), tn_(0), pmembers_(0),
    members_(0), senders_(0), rtcp_bandwidth_(0),
    we_sent_(0), avg_rtcp_pkt_pize_(0), rtcp_pkt_count_(0),
    initial_(true), active_(false), num_receivers_(0)
{
    cname_ = "hello"; //generate_cname();
    ssrc_  = ssrc;

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

    memset(&p->stats, 0, sizeof(struct statistics));

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

void kvz_rtp::rtcp::add_participant(uint32_t ssrc)
{
    participants_[ssrc] = initial_peers_.back();
    initial_peers_.pop_back();
    num_receivers_++;
}

void kvz_rtp::rtcp::update_rtcp_bandwidth(size_t pkt_size)
{
    rtcp_pkt_count_++;
    rtcp_byte_count_  += pkt_size + UDP_HEADER_SIZE + IP_HEADER_SIZE;
    avg_rtcp_pkt_pize_ = rtcp_byte_count_ / rtcp_pkt_count_;
}

void kvz_rtp::rtcp::set_sender_ssrc(sockaddr_in& addr, uint32_t ssrc)
{
    (void)addr;

    if (participants_.find(ssrc) != participants_.end()) {
        LOG_ERROR("SSRC clash detected, must be resolved!");
        return;
    }

    /* TODO: this is not correct, find the sender from initial_peers_ (TODO: how??) */
    add_participant(ssrc);
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

bool kvz_rtp::rtcp::is_participant(uint32_t ssrc)
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

void kvz_rtp::rtcp::sender_inc_seq_cycle_count()
{
    sender_stats.cycles_cnt++;
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
    if (is_participant(sender_ssrc)) {
        participants_[sender_ssrc]->stats.sent_bytes += n;
        return;
    }

    LOG_WARN("Got RTP packet from unknown source: 0x%x", sender_ssrc);
}

void kvz_rtp::rtcp::receiver_inc_sent_pkts(uint32_t sender_ssrc, size_t n)
{
    if (is_participant(sender_ssrc)) {
        participants_[sender_ssrc]->stats.sent_pkts += n;
        return;
    }

    LOG_WARN("Got RTP packet from unknown source: 0x%x", sender_ssrc);
}

void kvz_rtp::rtcp::receiver_update_stats(kvz_rtp::frame::rtp_frame *frame)
{
    if (!frame)
        return;

    if (!is_participant(frame->ssrc)) {
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

    for (uint32_t& ssrc : ssrcs) {
        if (!is_participant(ssrc)) {
            LOG_WARN("Unknown participant 0x%x", ssrc);
            continue;
        }

        auto p = participants_[ssrc];
        if ((ret = p->socket->sendto(p->address, (uint8_t *)frame, len, 0)) != RTP_OK) {
            LOG_ERROR("sendto() failed!");
        }

        update_rtcp_bandwidth(len);
    }

    return ret;
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

        update_rtcp_bandwidth(len);
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

        update_rtcp_bandwidth(len);
    }

    return ret;
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

        update_rtcp_bandwidth(len);
    }

    return ret;
}

rtp_error_t kvz_rtp::rtcp::send_app_packet(kvz_rtp::frame::rtcp_app_frame *frame)
{
    if (!frame)
        return RTP_INVALID_VALUE;

    uint16_t len  = frame->length;
    uint32_t ssrc = frame->ssrc;

    frame->length = htons(frame->length);
    frame->ssrc   = htonl(frame->ssrc);

    if (!is_participant(ssrc)) {
        LOG_ERROR("Unknown participant 0x%x", ssrc);
        return RTP_INVALID_VALUE;
    }

    rtp_error_t ret = participants_[ssrc]->socket->sendto((uint8_t *)frame, len, 0, NULL);

    if (ret == RTP_OK)
        update_rtcp_bandwidth(len);

    return ret;
}

rtp_error_t kvz_rtp::rtcp::generate_sender_report()
{
    return RTP_OK;

    if (num_receivers_ == 0)
        return RTP_NOT_READY;

    kvz_rtp::frame::rtcp_sender_frame *frame;

    if ((frame = kvz_rtp::frame::alloc_rtcp_sender_frame(num_receivers_)) == nullptr) {
        LOG_ERROR("Failed to allocate RTCP Receiver Report frame!");
        return rtp_errno;
    }

    size_t ptr         = 0;
    uint64_t timestamp = tv_to_ntp();
    rtp_error_t ret    = RTP_GENERIC_ERROR;

    frame->header.count    = num_receivers_;
    frame->sender_ssrc     = ssrc_;
    frame->s_info.ntp_msw  = timestamp >> 32;
    frame->s_info.ntp_lsw  = timestamp & 0xffffffff;
    frame->s_info.rtp_ts   = 3; /* TODO: what timestamp is this? */
    frame->s_info.pkt_cnt  = sender_stats.sent_pkts;
    frame->s_info.byte_cnt = sender_stats.sent_bytes;

    LOG_DEBUG("Sender Report from 0x%x has %u blocks", ssrc_, num_receivers_);

    for (auto& participant : participants_) {
        frame->blocks[ptr].ssrc = participant.first;

        if (participant.second->stats.dropped_pkts != 0) {
            frame->blocks[ptr].fraction =
                participant.second->stats.sent_pkts / participant.second->stats.dropped_pkts;
        }

        frame->blocks[ptr].lost     = participant.second->stats.dropped_pkts;
        frame->blocks[ptr].last_seq = participant.second->stats.highest_seq;
        frame->blocks[ptr].jitter   = participant.second->stats.jitter;
        frame->blocks[ptr].lsr      = participant.second->stats.lsr_ts;
        frame->blocks[ptr].dlsr     = participant.second->stats.lsr_delay;

        ptr++;
    }

    ret = kvz_rtp::rtcp::send_sender_report_packet(frame);
    (void)kvz_rtp::frame::dealloc_frame(frame);

    return RTP_OK;
}

rtp_error_t kvz_rtp::rtcp::generate_receiver_report()
{
    /* It is possible that haven't yet received an RTP packet from remote */
    if (num_receivers_ == 0) {
        LOG_WARN("cannot send receiver report yet, haven't received anything");
        return RTP_NOT_READY;
    }

    size_t ptr = 0;
    rtp_error_t ret;
    kvz_rtp::frame::rtcp_receiver_frame *frame;

    if ((frame = kvz_rtp::frame::alloc_rtcp_receiver_frame(num_receivers_)) == nullptr) {
        LOG_ERROR("Failed to allocate RTCP Receiver Report frame!");
        return rtp_errno;
    }

    frame->header.count = num_receivers_;
    frame->sender_ssrc  = ssrc_;

    LOG_DEBUG("Receiver Report from 0x%x has %u blocks", ssrc_, num_receivers_);

    for (auto& participant : participants_) {
        frame->blocks[ptr].ssrc = participant.first;

        if (participant.second->stats.dropped_pkts != 0) {
            frame->blocks[ptr].fraction =
                participant.second->stats.sent_pkts / participant.second->stats.dropped_pkts;
        }

        frame->blocks[ptr].lost     = participant.second->stats.dropped_pkts;
        frame->blocks[ptr].last_seq = participant.second->stats.highest_seq;
        frame->blocks[ptr].jitter   = participant.second->stats.jitter;
        frame->blocks[ptr].lsr      = participant.second->stats.lsr_ts;
        frame->blocks[ptr].dlsr     = participant.second->stats.lsr_delay;

        ptr++;
    }

    ret = kvz_rtp::rtcp::send_receiver_report_packet(frame);
    (void)kvz_rtp::frame::dealloc_frame(frame);

    return RTP_OK;
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

    frame->header.length = ntohs(frame->header.length);
    frame->sender_ssrc   = ntohl(frame->sender_ssrc);

    /* Receiver Reports are sent from participant that don't send RTP packets
     * This means that the sender of this report is not in the participants_ map
     * but rather in the initial_peers_ vector
     *
     * Check if that's the case and if so, move the entry from initial_peers_ to participants_ */
    if (!is_participant(frame->sender_ssrc)) {
        /* TODO: this is not correct way to do it! fix before multicast */
        add_participant(frame->sender_ssrc);
    }

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

    for (size_t i = 0; i < frame->header.count; ++i) {
        uint32_t ssrc = ntohl(frame->ssrc[i]);

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

    update_rtcp_bandwidth(size);

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
