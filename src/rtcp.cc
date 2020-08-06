#ifdef _WIN32
#else
#include <sys/time.h>
#endif

#include <cassert>
#include <cstdlib>
#include <cstring>
#include <iostream>

#include "debug.hh"
#include "hostname.hh"
#include "poll.hh"
#include "rtcp.hh"
#include "util.hh"

/* TODO: Find the actual used sizes somehow? */
#define UDP_HEADER_SIZE  8
#define IP_HEADER_SIZE  20

uvg_rtp::rtcp::rtcp(uint32_t ssrc, bool receiver):
    receiver_(receiver),
    tp_(0), tc_(0), tn_(0), pmembers_(0),
    members_(0), senders_(0), rtcp_bandwidth_(0),
    we_sent_(0), avg_rtcp_pkt_pize_(0), rtcp_pkt_count_(0),
    initial_(true), num_receivers_(0)
{
    ssrc_  = ssrc;

    clock_start_  = 0;
    clock_rate_   = 0;
    rtp_ts_start_ = 0;
    runner_       = nullptr;

    zero_stats(&sender_stats);
}

uvg_rtp::rtcp::rtcp(uvg_rtp::rtp *rtp)
{
}

uvg_rtp::rtcp::~rtcp()
{
}

rtp_error_t uvg_rtp::rtcp::add_participant(std::string dst_addr, int dst_port, int src_port, uint32_t clock_rate)
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

    zero_stats(&p->stats);

    if ((p->socket = new uvg_rtp::socket(RTP_CTX_NO_FLAGS)) == nullptr)
        return RTP_MEMORY_ERROR;

    if ((ret = p->socket->init(AF_INET, SOCK_DGRAM, 0)) != RTP_OK)
        return ret;

    int enable = 1;

    if ((ret = p->socket->setsockopt(SOL_SOCKET, SO_REUSEADDR, (const char *)&enable, sizeof(int))) != RTP_OK)
        return ret;

#ifdef _WIN32
    /* Make the socket non-blocking */
    int enabled = 1;

    if (::ioctlsocket(p->socket->get_raw_socket(), FIONBIO, (u_long *)&enabled) < 0)
        LOG_ERROR("Failed to make the socket non-blocking!");
#endif

    /* Set read timeout (5s for now) 
     *
     * This means that the socket is listened for 5s at a time and after the timeout, 
     * Send Report is sent to all participants */
    struct timeval tv;
    tv.tv_sec = 3;
    tv.tv_usec = 0;

    if ((ret = p->socket->setsockopt(SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv))) != RTP_OK)
        return ret;

    LOG_WARN("Binding to port %d (source port)", src_port);

    if ((ret = p->socket->bind(AF_INET, INADDR_ANY, src_port)) != RTP_OK)
        return ret;

    p->address = p->socket->create_sockaddr(AF_INET, dst_addr, dst_port);
    p->sender  = false;
    p->stats.clock_rate = clock_rate;

    initial_participants_.push_back(p);
    sockets_.push_back(*p->socket);

    return RTP_OK;
}

void uvg_rtp::rtcp::add_participant(uint32_t ssrc)
{
    participants_[ssrc] = initial_participants_.back();
    initial_participants_.pop_back();
    num_receivers_++;

    participants_[ssrc]->r_frame    = nullptr;
    participants_[ssrc]->s_frame    = nullptr;
    participants_[ssrc]->sdes_frame = nullptr;
    participants_[ssrc]->app_frame  = nullptr;
}

void uvg_rtp::rtcp::update_rtcp_bandwidth(size_t pkt_size)
{
    rtcp_pkt_count_++;
    rtcp_byte_count_  += pkt_size + UDP_HEADER_SIZE + IP_HEADER_SIZE;
    avg_rtcp_pkt_pize_ = rtcp_byte_count_ / rtcp_pkt_count_;
}


void uvg_rtp::rtcp::zero_stats(uvg_rtp::rtcp::statistics *stats)
{
    stats->received_pkts  = 0;
    stats->dropped_pkts   = 0;
    stats->received_bytes = 0;

    stats->sent_pkts  = 0;
    stats->sent_bytes = 0;

    stats->jitter  = 0;
    stats->transit = 0;

    stats->initial_ntp = 0;
    stats->initial_rtp = 0;
    stats->clock_rate  = 0;
    stats->lsr         = 0;

    stats->max_seq  = 0;
    stats->base_seq = 0;
    stats->bad_seq  = 0;
    stats->cycles   = 0;
}

rtp_error_t uvg_rtp::rtcp::start()
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

rtp_error_t uvg_rtp::rtcp::stop()
{
    if (runner_ == nullptr)
        goto free_mem;

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
    /* Send BYE packet with our SSRC to all participants */
    uvg_rtp::rtcp::terminate_self();

free_mem:
    /* free all receiver statistic structs */
    for (auto& participant : participants_) {
        delete participant.second->socket;
        delete participant.second;
    }

    return RTP_OK;
}

bool uvg_rtp::rtcp::receiver() const
{
    return receiver_;
}

std::vector<uvg_rtp::socket>& uvg_rtp::rtcp::get_sockets()
{
    return sockets_;
}

std::vector<uint32_t> uvg_rtp::rtcp::get_participants()
{
    std::vector<uint32_t> ssrcs;

    for (auto& i : participants_) {
        ssrcs.push_back(i.first);
    }

    return ssrcs;
}

bool uvg_rtp::rtcp::is_participant(uint32_t ssrc)
{
    return participants_.find(ssrc) != participants_.end();
}

void uvg_rtp::rtcp::sender_inc_sent_bytes(size_t n)
{
    sender_stats.sent_bytes += n;
}

void uvg_rtp::rtcp::sender_inc_sent_pkts(size_t n)
{
    sender_stats.sent_pkts += n;
}

void uvg_rtp::rtcp::sender_inc_seq_cycle_count()
{
    sender_stats.cycles++;
}

void uvg_rtp::rtcp::set_sender_ts_info(uint64_t clock_start, uint32_t clock_rate, uint32_t rtp_ts_start)
{
    clock_start_  = clock_start;
    clock_rate_   = clock_rate;
    rtp_ts_start_ = rtp_ts_start;
}

void uvg_rtp::rtcp::sender_update_stats(uvg_rtp::frame::rtp_frame *frame)
{
    if (!frame)
        return;

    sender_stats.sent_pkts  += 1;
    sender_stats.sent_bytes += frame->payload_len;
    sender_stats.max_seq     = frame->header.seq;
}

void uvg_rtp::rtcp::receiver_inc_sent_bytes(uint32_t sender_ssrc, size_t n)
{
    if (is_participant(sender_ssrc)) {
        participants_[sender_ssrc]->stats.sent_bytes += n;
        return;
    }

    LOG_WARN("Got RTP packet from unknown source: 0x%x", sender_ssrc);
}

void uvg_rtp::rtcp::receiver_inc_sent_pkts(uint32_t sender_ssrc, size_t n)
{
    if (is_participant(sender_ssrc)) {
        participants_[sender_ssrc]->stats.sent_pkts += n;
        return;
    }

    LOG_WARN("Got RTP packet from unknown source: 0x%x", sender_ssrc);
}

void uvg_rtp::rtcp::init_new_participant(uvg_rtp::frame::rtp_frame *frame)
{
    assert(frame != nullptr);

    uvg_rtp::rtcp::add_participant(frame->header.ssrc);
    uvg_rtp::rtcp::init_participant_seq(frame->header.ssrc, frame->header.seq);

    /* Set the probation to MIN_SEQUENTIAL (2)
     *
     * What this means is that we must receive at least two packets from SSRC
     * with sequential RTP sequence numbers for this peer to be considered valid */
    participants_[frame->header.ssrc]->probation = MIN_SEQUENTIAL;

    /* This is the first RTP frame from remote to frame->header.timestamp represents t = 0
     * Save the timestamp and current NTP timestamp so we can do jitter calculations later on */
    participants_[frame->header.ssrc]->stats.initial_rtp = frame->header.timestamp;
    participants_[frame->header.ssrc]->stats.initial_ntp = uvg_rtp::clock::ntp::now();

    senders_++;
}

void uvg_rtp::rtcp::init_participant_seq(uint32_t ssrc, uint16_t base_seq)
{
    if (participants_.find(ssrc) == participants_.end())
        return;

    participants_[ssrc]->stats.base_seq = base_seq;
    participants_[ssrc]->stats.max_seq  = base_seq;
    participants_[ssrc]->stats.bad_seq  = (uint16_t)RTP_SEQ_MOD + 1;
}

rtp_error_t uvg_rtp::rtcp::update_participant_seq(uint32_t ssrc, uint16_t seq)
{
    if (participants_.find(ssrc) == participants_.end())
        return RTP_GENERIC_ERROR;

    auto p = participants_[ssrc];
    uint16_t udelta = seq - p->stats.max_seq;

    /* Source is not valid until MIN_SEQUENTIAL packets with
    * sequential sequence numbers have been received.  */
    if (p->probation) {
       /* packet is in sequence */
       if (seq == p->stats.max_seq + 1) {
           p->probation--;
           p->stats.max_seq = seq;
           if (p->probation == 0) {
               uvg_rtp::rtcp::init_participant_seq(ssrc, seq);
               return RTP_OK;
            }
       } else {
           p->probation = MIN_SEQUENTIAL - 1;
           p->stats.max_seq = seq;
       }
       return RTP_GENERIC_ERROR;
    } else if (udelta < MAX_DROPOUT) {
       /* in order, with permissible gap */
       if (seq < p->stats.max_seq) {
           /* Sequence number wrapped - count another 64K cycle.  */
           p->stats.cycles += RTP_SEQ_MOD;
       }
       p->stats.max_seq = seq;
    } else if (udelta <= RTP_SEQ_MOD - MAX_MISORDER) {
       /* the sequence number made a very large jump */
       if (seq == p->stats.bad_seq) {
           /* Two sequential packets -- assume that the other side
            * restarted without telling us so just re-sync
            * (i.e., pretend this was the first packet).  */
           uvg_rtp::rtcp::init_participant_seq(ssrc, seq);
       }
       else {
           p->stats.bad_seq = (seq + 1) & (RTP_SEQ_MOD - 1);
           return RTP_GENERIC_ERROR;
       }
    } else {
       /* duplicate or reordered packet */
    }

    return RTP_OK;
}

rtp_error_t uvg_rtp::rtcp::terminate_self()
{
    rtp_error_t ret;
    auto bye_frame = uvg_rtp::frame::alloc_rtcp_bye_frame(1);

    bye_frame->ssrc[0] = ssrc_;

    if ((ret = send_bye_packet(bye_frame)) != RTP_OK) {
        LOG_ERROR("Failed to send BYE");
    }

    (void)uvg_rtp::frame::dealloc_frame(bye_frame);

    return ret;
}

rtp_error_t uvg_rtp::rtcp::reset_rtcp_state(uint32_t ssrc)
{
    if (participants_.find(ssrc) != participants_.end())
        return RTP_SSRC_COLLISION;

    sender_stats.received_pkts  = 0;
    sender_stats.dropped_pkts   = 0;
    sender_stats.received_bytes = 0;
    sender_stats.sent_pkts      = 0;
    sender_stats.sent_bytes     = 0;
    sender_stats.jitter         = 0;
    sender_stats.transit        = 0;
    sender_stats.max_seq        = 0;
    sender_stats.base_seq       = 0;
    sender_stats.bad_seq        = 0;
    sender_stats.cycles         = 0;

    return RTP_OK;
}

bool uvg_rtp::rtcp::collision_detected(uint32_t ssrc, sockaddr_in& src_addr)
{
    if (participants_.find(ssrc) == participants_.end())
        return false;

    auto sender = participants_[ssrc];

    if (src_addr.sin_port        != sender->address.sin_port &&
        src_addr.sin_addr.s_addr != sender->address.sin_addr.s_addr)
        return true;

    return false;
}

rtp_error_t uvg_rtp::rtcp::receiver_update_stats(uvg_rtp::frame::rtp_frame *frame)
{
    if (!frame)
        return RTP_OK;

    if (uvg_rtp::rtcp::collision_detected(frame->header.ssrc, frame->src_addr)) {
        LOG_WARN("collision detected, packet must be dropped");

        /* check if the SSRC of remote is ours, we need to send RTCP BYE
         * and reinitialize ourselves */
        if (frame->header.ssrc == ssrc_) {
            terminate_self();
            participants_.erase(ssrc_);
            return RTP_SSRC_COLLISION;
        }

        return RTP_INVALID_VALUE;
    }

    /* RTCP runner is not running so we don't need to do any other checks,
     * just create new participant so we can check for SSRC collisions */
    if (runner_ == nullptr) {
        if (participants_.find(frame->header.ssrc) == participants_.end()) {
            auto participant = new struct participant;
            participant->address = frame->src_addr;
            participant->socket  = nullptr;
            participants_[frame->header.ssrc] = participant;
        }

        return RTP_OK;
    }

    if (!uvg_rtp::rtcp::is_participant(frame->header.ssrc)) {
        LOG_WARN("Got RTP packet from unknown source: 0x%x", frame->header.ssrc);
        init_new_participant(frame);
    }

    if (uvg_rtp::rtcp::update_participant_seq(frame->header.ssrc, frame->header.seq) != RTP_OK) {
        LOG_WARN("Invalid packet received from remote!");
        return RTP_INVALID_VALUE;
    }

    auto p = participants_[frame->header.ssrc];

    p->stats.received_pkts  += 1;
    p->stats.received_bytes += frame->payload_len;

    /* calculate number of dropped packets */
    int extended_max = p->stats.cycles + p->stats.max_seq;
    int expected     = extended_max - p->stats.base_seq + 1;

    p->stats.dropped_pkts = expected - p->stats.received_pkts;

    int arrival =
        p->stats.initial_rtp +
        + uvg_rtp::clock::ntp::diff_now(p->stats.initial_ntp)
        * (p->stats.clock_rate
        / 1000);

	/* calculate interarrival jitter */
    int transit = arrival - frame->header.timestamp;
    int d = std::abs((int)(transit - p->stats.transit));

    p->stats.transit = transit;
    p->stats.jitter += (1.f / 16.f) * ((double)d - p->stats.jitter);

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

        if ((ret = p->socket->sendto(p->address, (uint8_t *)frame, len, 0)) != RTP_OK) {
            LOG_ERROR("sendto() failed!");
            return ret;
        }

        update_rtcp_bandwidth(len);
    }

    return ret;
}

rtp_error_t uvg_rtp::rtcp::send_bye_packet(uvg_rtp::frame::rtcp_bye_frame *frame)
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

rtp_error_t uvg_rtp::rtcp::send_sdes_packet(uvg_rtp::frame::rtcp_sdes_frame *frame)
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

rtp_error_t uvg_rtp::rtcp::send_app_packet(uvg_rtp::frame::rtcp_app_frame *frame)
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

uvg_rtp::frame::rtcp_sender_frame *uvg_rtp::rtcp::get_sender_packet(uint32_t ssrc)
{
    if (participants_.find(ssrc) == participants_.end())
        return nullptr;

    auto frame = participants_[ssrc]->s_frame;
    participants_[ssrc]->s_frame = nullptr;

    return frame;
}

uvg_rtp::frame::rtcp_receiver_frame *uvg_rtp::rtcp::get_receiver_packet(uint32_t ssrc)
{
    if (participants_.find(ssrc) == participants_.end())
        return nullptr;

    auto frame = participants_[ssrc]->r_frame;
    participants_[ssrc]->r_frame = nullptr;

    return frame;
}

uvg_rtp::frame::rtcp_sdes_frame *uvg_rtp::rtcp::get_sdes_packet(uint32_t ssrc)
{
    if (participants_.find(ssrc) == participants_.end())
        return nullptr;

    auto frame = participants_[ssrc]->sdes_frame;
    participants_[ssrc]->sdes_frame = nullptr;

    return frame;
}

uvg_rtp::frame::rtcp_app_frame *uvg_rtp::rtcp::get_app_packet(uint32_t ssrc)
{
    if (participants_.find(ssrc) == participants_.end())
        return nullptr;

    auto frame = participants_[ssrc]->app_frame;
    participants_[ssrc]->app_frame = nullptr;

    return frame;
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
    frame->s_info.pkt_cnt  = sender_stats.sent_pkts;
    frame->s_info.byte_cnt = sender_stats.sent_bytes;

    LOG_DEBUG("Sender Report from 0x%x has %zu blocks", ssrc_, senders_);

    for (auto& participant : participants_) {
        if (!participant.second->sender)
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

rtp_error_t uvg_rtp::rtcp::generate_report()
{
    if (receiver_)
        return generate_receiver_report();
    return generate_sender_report();
}

rtp_error_t uvg_rtp::rtcp::install_sender_hook(void (*hook)(uvg_rtp::frame::rtcp_sender_frame *))
{
    if (!hook)
        return RTP_INVALID_VALUE;

    sender_hook_ = hook;
    return RTP_OK;
}

rtp_error_t uvg_rtp::rtcp::install_receiver_hook(void (*hook)(uvg_rtp::frame::rtcp_receiver_frame *))
{
    if (!hook)
        return RTP_INVALID_VALUE;

    receiver_hook_ = hook;
    return RTP_OK;
}

rtp_error_t uvg_rtp::rtcp::install_sdes_hook(void (*hook)(uvg_rtp::frame::rtcp_sdes_frame *))
{
    if (!hook)
        return RTP_INVALID_VALUE;

    sdes_hook_ = hook;
    return RTP_OK;
}

rtp_error_t uvg_rtp::rtcp::install_app_hook(void (*hook)(uvg_rtp::frame::rtcp_app_frame *))
{
    if (!hook)
        return RTP_INVALID_VALUE;

    app_hook_ = hook;
    return RTP_OK;
}

rtp_error_t uvg_rtp::rtcp::handle_sender_report_packet(uvg_rtp::frame::rtcp_sender_frame *frame, size_t size)
{
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
        cpy_frame->blocks[i].lost     = ntohl(cpy_frame->blocks[i].lost);
        cpy_frame->blocks[i].last_seq = ntohl(cpy_frame->blocks[i].last_seq);
        cpy_frame->blocks[i].lsr      = ntohl(cpy_frame->blocks[i].lsr);
        cpy_frame->blocks[i].dlsr     = ntohl(cpy_frame->blocks[i].dlsr);

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
        cpy_frame->blocks[i].lost     = ntohl(cpy_frame->blocks[i].lost);
        cpy_frame->blocks[i].last_seq = ntohl(cpy_frame->blocks[i].last_seq);
        cpy_frame->blocks[i].jitter   = ntohl(cpy_frame->blocks[i].jitter);
        cpy_frame->blocks[i].lsr      = ntohl(cpy_frame->blocks[i].lsr);
        cpy_frame->blocks[i].dlsr     = ntohl(cpy_frame->blocks[i].dlsr);

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

rtp_error_t uvg_rtp::rtcp::handle_sdes_packet(uvg_rtp::frame::rtcp_sdes_frame *frame, size_t size)
{
    if (!frame)
        return RTP_INVALID_VALUE;

    if (frame->header.count == 0) {
        LOG_ERROR("SDES packet cannot contain 0 fields!");
        return RTP_INVALID_VALUE;
    }

    frame->sender_ssrc = ntohl(frame->sender_ssrc);

    /* We need to make a copy of the frame because right now frame points to RTCP recv buffer
     * Deallocate previous frame if it exists */
    if (participants_[frame->sender_ssrc]->sdes_frame != nullptr)
        (void)uvg_rtp::frame::dealloc_frame(participants_[frame->sender_ssrc]->sdes_frame);

    uint8_t *cpy_frame = new uint8_t[size];
    memcpy(cpy_frame, frame, size);

    if (sdes_hook_)
        sdes_hook_((uvg_rtp::frame::rtcp_sdes_frame *)cpy_frame);
    else
        participants_[frame->sender_ssrc]->sdes_frame = (uvg_rtp::frame::rtcp_sdes_frame *)cpy_frame;

    return RTP_OK;
}

rtp_error_t uvg_rtp::rtcp::handle_bye_packet(uvg_rtp::frame::rtcp_bye_frame *frame, size_t size)
{
    (void)size;

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

rtp_error_t uvg_rtp::rtcp::handle_app_packet(uvg_rtp::frame::rtcp_app_frame *frame, size_t size)
{
    if (!frame)
        return RTP_INVALID_VALUE;

    frame->ssrc   = ntohl(frame->ssrc);
    frame->length = ntohs(frame->length);

    /* We need to make a copy of the frame because right now frame points to RTCP recv buffer
     * Deallocate previous frame if it exists */
    if (participants_[frame->ssrc]->app_frame != nullptr)
        (void)uvg_rtp::frame::dealloc_frame(participants_[frame->ssrc]->app_frame);

    uint8_t *cpy_frame = new uint8_t[size];
    memcpy(cpy_frame, frame, size);

    if (app_hook_)
        app_hook_((uvg_rtp::frame::rtcp_app_frame *)cpy_frame);
    else
        participants_[frame->ssrc]->app_frame = (uvg_rtp::frame::rtcp_app_frame *)cpy_frame;

    return RTP_OK;
}

rtp_error_t uvg_rtp::rtcp::handle_incoming_packet(uint8_t *buffer, size_t size)
{
    (void)size;

    uvg_rtp::frame::rtcp_header *header = (uvg_rtp::frame::rtcp_header *)buffer;

    if (header->version != 2) {
        LOG_ERROR("Invalid header version (%u)", header->version);
        return RTP_INVALID_VALUE;
    }

    if (header->padding != 0) {
        LOG_ERROR("Cannot handle padded packets!");
        return RTP_INVALID_VALUE;
    }
    
    if (header->pkt_type > uvg_rtp::frame::RTCP_FT_BYE ||
        header->pkt_type < uvg_rtp::frame::RTCP_FT_SR) {
        LOG_ERROR("Invalid packet type (%u)!", header->pkt_type);
        return RTP_INVALID_VALUE;
    }

    update_rtcp_bandwidth(size);

    rtp_error_t ret = RTP_INVALID_VALUE;

    switch (header->pkt_type) {
        case uvg_rtp::frame::RTCP_FT_SR:
            ret = handle_sender_report_packet((uvg_rtp::frame::rtcp_sender_frame *)buffer, size);
            break;

        case uvg_rtp::frame::RTCP_FT_RR:
            ret = handle_receiver_report_packet((uvg_rtp::frame::rtcp_receiver_frame *)buffer, size);
            break;

        case uvg_rtp::frame::RTCP_FT_SDES:
            ret = handle_sdes_packet((uvg_rtp::frame::rtcp_sdes_frame *)buffer, size);
            break;

        case uvg_rtp::frame::RTCP_FT_BYE:
            ret = handle_bye_packet((uvg_rtp::frame::rtcp_bye_frame *)buffer, size);
            break;

        case uvg_rtp::frame::RTCP_FT_APP:
            ret = handle_app_packet((uvg_rtp::frame::rtcp_app_frame *)buffer, size);
            break;
    }

    return ret;
}

void uvg_rtp::rtcp::rtcp_runner(uvg_rtp::rtcp *rtcp)
{
    LOG_INFO("RTCP instance created!");

    uvg_rtp::clock::hrc::hrc_t start, end;
    int nread, diff, timeout = MIN_TIMEOUT;
    uint8_t buffer[MAX_PACKET];
    rtp_error_t ret;

    while (rtcp->active()) {
        start = uvg_rtp::clock::hrc::now();
        ret   = uvg_rtp::poll::poll(rtcp->get_sockets(), buffer, MAX_PACKET, timeout, &nread);

        if (ret == RTP_OK && nread > 0) {
            (void)rtcp->handle_incoming_packet(buffer, (size_t)nread);
        } else if (ret == RTP_INTERRUPTED) {
            /* do nothing */
        } else {
            LOG_ERROR("recvfrom failed, %d", ret);
        }

        diff = uvg_rtp::clock::hrc::diff_now(start);

        if (diff >= MIN_TIMEOUT) {
            if ((ret = rtcp->generate_report()) != RTP_OK && ret != RTP_NOT_READY) {
                LOG_ERROR("Failed to send RTCP status report!");
            }

            timeout = MIN_TIMEOUT;
        }
    }
}

rtp_error_t uvg_rtp::rtcp::packet_handler(int flags, frame::rtp_frame **out)
{
    return RTP_PKT_NOT_HANDLED;
}
