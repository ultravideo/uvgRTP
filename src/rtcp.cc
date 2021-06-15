#include "rtcp.hh"

#include "hostname.hh"
#include "poll.hh"
#include "debug.hh"
#include "util.hh"
#include "rtp.hh"
#include "frame.hh"
#include "srtp/srtcp.hh"

#ifndef _WIN32
#include <sys/time.h>
#endif

#include <cassert>
#include <cstdlib>
#include <cstring>
#include <iostream>

const uint16_t RTCP_HEADER_SIZE = 4;
const uint16_t SSRC_CSRC_SIZE = 4;
const uint16_t SENDER_INFO_SIZE = 20;
const uint16_t REPORT_BLOCK_SIZE = 24;
const uint16_t APP_NAME_SIZE = 4;

const uint32_t MAX_SUPPORTED_PARTICIPANTS = 31;

uvgrtp::rtcp::rtcp(uvgrtp::rtp *rtp, int flags):
    rtp_(rtp), flags_(flags), our_role_(RECEIVER),
    tp_(0), tc_(0), tn_(0), pmembers_(0),
    members_(0), senders_(0), rtcp_bandwidth_(0),
    we_sent_(0), avg_rtcp_pkt_pize_(0), rtcp_pkt_count_(0),
    rtcp_pkt_sent_count_(0), initial_(true), num_receivers_(0),
    sender_hook_(nullptr),
    receiver_hook_(nullptr),
    sdes_hook_(nullptr),
    app_hook_(nullptr),
    sr_hook_f_(nullptr),
    rr_hook_f_(nullptr),
    sdes_hook_f_(nullptr),
    app_hook_f_(nullptr)
{
    ssrc_         = rtp->get_ssrc();
    clock_rate_   = rtp->get_clock_rate();

    clock_start_  = 0;
    rtp_ts_start_ = 0;
    runner_       = nullptr;
    srtcp_        = nullptr;

    zero_stats(&our_stats);
}

uvgrtp::rtcp::rtcp(uvgrtp::rtp *rtp, uvgrtp::srtcp *srtcp, int flags):
    rtcp(rtp, flags)
{
    srtcp_ = srtcp;
}

uvgrtp::rtcp::~rtcp()
{
}

rtp_error_t uvgrtp::rtcp::start()
{
    if (sockets_.empty()) {
        LOG_ERROR("Cannot start RTCP Runner because no connections have been initialized");
        return RTP_INVALID_VALUE;
    }
    active_ = true;

    runner_ = new std::thread(rtcp_runner, this);
    runner_->detach();

    return RTP_OK;
}

rtp_error_t uvgrtp::rtcp::stop()
{
    if (!runner_)
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
    return uvgrtp::rtcp::send_bye_packet({ ssrc_ });

free_mem:
    /* free all receiver statistic structs */
    for (auto& participant : participants_) {
        delete participant.second->socket;
        delete participant.second;
    }

    return RTP_OK;
}

void uvgrtp::rtcp::rtcp_runner(uvgrtp::rtcp* rtcp)
{
    LOG_INFO("RTCP instance created!");

    uvgrtp::clock::hrc::hrc_t start, end;
    int nread, diff, timeout = MIN_TIMEOUT;
    uint8_t buffer[MAX_PACKET];
    rtp_error_t ret;

    while (rtcp->active()) {
        start = uvgrtp::clock::hrc::now();
        ret = uvgrtp::poll::poll(rtcp->get_sockets(), buffer, MAX_PACKET, timeout, &nread);

        if (ret == RTP_OK && nread > 0) {
            (void)rtcp->handle_incoming_packet(buffer, (size_t)nread);
        }
        else if (ret == RTP_INTERRUPTED) {
            /* do nothing */
        }
        else {
            LOG_ERROR("recvfrom failed, %d", ret);
        }

        diff = (int)uvgrtp::clock::hrc::diff_now(start);

        if (diff >= MIN_TIMEOUT) {
            if ((ret = rtcp->generate_report()) != RTP_OK && ret != RTP_NOT_READY) {
                LOG_ERROR("Failed to send RTCP status report!");
            }

            timeout = MIN_TIMEOUT;
        }
    }
}

rtp_error_t uvgrtp::rtcp::add_participant(std::string dst_addr, uint16_t dst_port, uint16_t src_port, uint32_t clock_rate)
{
    if (dst_addr == "" || !dst_port || !src_port) {
        LOG_ERROR("Invalid values given (%s, %d, %d), cannot create RTCP instance",
                dst_addr.c_str(), dst_port, src_port);
        return RTP_INVALID_VALUE;
    }

    rtp_error_t ret;
    rtcp_participant *p;

    p = new rtcp_participant();

    zero_stats(&p->stats);

    p->socket = new uvgrtp::socket(0);

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

    p->role             = RECEIVER;
    p->address          = p->socket->create_sockaddr(AF_INET, dst_addr, dst_port);
    p->stats.clock_rate = clock_rate;

    initial_participants_.push_back(p);
    sockets_.push_back(*p->socket);

    return RTP_OK;
}

rtp_error_t uvgrtp::rtcp::add_participant(uint32_t ssrc)
{
    if (num_receivers_ == MAX_SUPPORTED_PARTICIPANTS)
    {
        LOG_ERROR("Maximum number of RTCP participants reached.");
        // TODO: Support more participants by sending the multiple messages at the same time
        return RTP_GENERIC_ERROR;
    }

    /* RTCP is not in use for this media stream,
     * create a "fake" participant that is only used for storing statistics information */
    if (initial_participants_.empty()) {
        participants_[ssrc] = new rtcp_participant();
        zero_stats(&participants_[ssrc]->stats);
    } else {
        participants_[ssrc] = initial_participants_.back();
        initial_participants_.pop_back();
    }
    num_receivers_++;

    participants_[ssrc]->rr_frame    = nullptr;
    participants_[ssrc]->sr_frame    = nullptr;
    participants_[ssrc]->sdes_frame = nullptr;
    participants_[ssrc]->app_frame  = nullptr;

    return RTP_OK;
}

rtp_error_t uvgrtp::rtcp::install_sender_hook(void (*hook)(uvgrtp::frame::rtcp_sender_report*))
{
    if (!hook)
        return RTP_INVALID_VALUE;

    sender_hook_ = hook;
    sr_hook_f_ = nullptr;
    return RTP_OK;
}

rtp_error_t uvgrtp::rtcp::install_sender_hook(std::function<void(std::shared_ptr<uvgrtp::frame::rtcp_sender_report>)> sr_handler)
{
    if (!sr_handler)
        return RTP_INVALID_VALUE;

    sender_hook_ = nullptr;
    sr_hook_f_ = sr_handler;
    return RTP_OK;
}

rtp_error_t uvgrtp::rtcp::install_receiver_hook(void (*hook)(uvgrtp::frame::rtcp_receiver_report*))
{
    if (!hook)
        return RTP_INVALID_VALUE;

    receiver_hook_ = hook;
    rr_hook_f_ = nullptr;
    return RTP_OK;
}

rtp_error_t uvgrtp::rtcp::install_receiver_hook(std::function<void(std::shared_ptr<uvgrtp::frame::rtcp_receiver_report>)> rr_handler)
{
    if (!rr_handler)
        return RTP_INVALID_VALUE;

    receiver_hook_ = nullptr;
    rr_hook_f_ = rr_handler;
    return RTP_OK;
}

rtp_error_t uvgrtp::rtcp::install_sdes_hook(void (*hook)(uvgrtp::frame::rtcp_sdes_packet*))
{
    if (!hook)
        return RTP_INVALID_VALUE;

    sdes_hook_ = hook;
    sdes_hook_f_ = nullptr;
    return RTP_OK;
}

rtp_error_t uvgrtp::rtcp::install_sdes_hook(std::function<void(std::shared_ptr<uvgrtp::frame::rtcp_sdes_packet>)> sdes_handler)
{
    if (!sdes_handler)
        return RTP_INVALID_VALUE;

    sdes_hook_ = nullptr;
    sdes_hook_f_ = sdes_handler;
    return RTP_OK;
}

rtp_error_t uvgrtp::rtcp::install_app_hook(void (*hook)(uvgrtp::frame::rtcp_app_packet*))
{
    if (!hook)
        return RTP_INVALID_VALUE;

    app_hook_ = hook;
    app_hook_f_ = nullptr;
    return RTP_OK;
}

rtp_error_t uvgrtp::rtcp::install_app_hook(std::function<void(std::shared_ptr<uvgrtp::frame::rtcp_app_packet>)> app_handler)
{
    if (!app_handler)
        return RTP_INVALID_VALUE;

    app_hook_ = nullptr;
    app_hook_f_ = app_handler;
    return RTP_OK;
}

uvgrtp::frame::rtcp_sender_report* uvgrtp::rtcp::get_sender_packet(uint32_t ssrc)
{
    if (participants_.find(ssrc) == participants_.end())
        return nullptr;

    auto frame = participants_[ssrc]->sr_frame;
    participants_[ssrc]->sr_frame = nullptr;

    return frame;
}

uvgrtp::frame::rtcp_receiver_report* uvgrtp::rtcp::get_receiver_packet(uint32_t ssrc)
{
    if (participants_.find(ssrc) == participants_.end())
        return nullptr;

    auto frame = participants_[ssrc]->rr_frame;
    participants_[ssrc]->rr_frame = nullptr;

    return frame;
}

uvgrtp::frame::rtcp_sdes_packet* uvgrtp::rtcp::get_sdes_packet(uint32_t ssrc)
{
    if (participants_.find(ssrc) == participants_.end())
        return nullptr;

    auto frame = participants_[ssrc]->sdes_frame;
    participants_[ssrc]->sdes_frame = nullptr;

    return frame;
}

uvgrtp::frame::rtcp_app_packet* uvgrtp::rtcp::get_app_packet(uint32_t ssrc)
{
    if (participants_.find(ssrc) == participants_.end())
        return nullptr;

    auto frame = participants_[ssrc]->app_frame;
    participants_[ssrc]->app_frame = nullptr;

    return frame;
}

std::vector<uvgrtp::socket>& uvgrtp::rtcp::get_sockets()
{
    return sockets_;
}

std::vector<uint32_t> uvgrtp::rtcp::get_participants()
{
    std::vector<uint32_t> ssrcs;

    for (auto& i : participants_) {
        ssrcs.push_back(i.first);
    }

    return ssrcs;
}

void uvgrtp::rtcp::update_rtcp_bandwidth(size_t pkt_size)
{
    rtcp_pkt_count_    += 1;
    rtcp_byte_count_   += pkt_size + UDP_HDR_SIZE + IPV4_HDR_SIZE;
    avg_rtcp_pkt_pize_  = rtcp_byte_count_ / rtcp_pkt_count_;
}

void uvgrtp::rtcp::zero_stats(uvgrtp::rtcp_statistics *stats)
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

bool uvgrtp::rtcp::is_participant(uint32_t ssrc)
{
    return participants_.find(ssrc) != participants_.end();
}

void uvgrtp::rtcp::set_ts_info(uint64_t clock_start, uint32_t clock_rate, uint32_t rtp_ts_start)
{
    clock_start_  = clock_start;
    clock_rate_   = clock_rate;
    rtp_ts_start_ = rtp_ts_start;
}

void uvgrtp::rtcp::sender_update_stats(uvgrtp::frame::rtp_frame *frame)
{
    if (!frame)
        return;

    if (frame->payload_len > UINT32_MAX)
    {
        LOG_ERROR("Payload size larger than uint32 max which is not supported by RFC 3550");
        return;
    }

    our_stats.sent_pkts  += 1;
    our_stats.sent_bytes += (uint32_t)frame->payload_len;
    our_stats.max_seq     = frame->header.seq;
}

rtp_error_t uvgrtp::rtcp::init_new_participant(uvgrtp::frame::rtp_frame *frame)
{
    rtp_error_t ret;

    if ((ret = uvgrtp::rtcp::add_participant(frame->header.ssrc)) != RTP_OK)
        return ret;

    if ((ret = uvgrtp::rtcp::init_participant_seq(frame->header.ssrc, frame->header.seq)) != RTP_OK)
        return ret;

    /* Set the probation to MIN_SEQUENTIAL (2)
     *
     * What this means is that we must receive at least two packets from SSRC
     * with sequential RTP sequence numbers for this peer to be considered valid */
    participants_[frame->header.ssrc]->probation = MIN_SEQUENTIAL;

    /* This is the first RTP frame from remote to frame->header.timestamp represents t = 0
     * Save the timestamp and current NTP timestamp so we can do jitter calculations later on */
    participants_[frame->header.ssrc]->stats.initial_rtp = frame->header.timestamp;
    participants_[frame->header.ssrc]->stats.initial_ntp = uvgrtp::clock::ntp::now();

    senders_++;

    return ret;
}

rtp_error_t uvgrtp::rtcp::update_sender_stats(size_t pkt_size)
{
    if (our_role_ == RECEIVER)
        our_role_ = SENDER;

    if (our_stats.sent_bytes + pkt_size > UINT32_MAX)
    {
        LOG_ERROR("Sent bytes overflow");
    }

    our_stats.sent_pkts  += 1;
    our_stats.sent_bytes += (uint32_t)pkt_size;

    return RTP_OK;
}

rtp_error_t uvgrtp::rtcp::init_participant_seq(uint32_t ssrc, uint16_t base_seq)
{
    if (participants_.find(ssrc) == participants_.end())
        return RTP_NOT_FOUND;

    participants_[ssrc]->stats.base_seq = base_seq;
    participants_[ssrc]->stats.max_seq  = base_seq;
    participants_[ssrc]->stats.bad_seq  = (RTP_SEQ_MOD + 1)%UINT32_MAX;

    return RTP_OK;
}

rtp_error_t uvgrtp::rtcp::update_participant_seq(uint32_t ssrc, uint16_t seq)
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
           if (!p->probation) {
               uvgrtp::rtcp::init_participant_seq(ssrc, seq);
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
           uvgrtp::rtcp::init_participant_seq(ssrc, seq);
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

rtp_error_t uvgrtp::rtcp::reset_rtcp_state(uint32_t ssrc)
{
    if (participants_.find(ssrc) != participants_.end())
        return RTP_SSRC_COLLISION;

    zero_stats(&our_stats);

    return RTP_OK;
}

bool uvgrtp::rtcp::collision_detected(uint32_t ssrc, sockaddr_in& src_addr)
{
    if (participants_.find(ssrc) == participants_.end())
        return false;

    auto sender = participants_[ssrc];

    if (src_addr.sin_port        != sender->address.sin_port &&
        src_addr.sin_addr.s_addr != sender->address.sin_addr.s_addr)
        return true;

    return false;
}

void uvgrtp::rtcp::update_session_statistics(uvgrtp::frame::rtp_frame *frame)
{
    auto p = participants_[frame->header.ssrc];

    p->stats.received_pkts  += 1;
    p->stats.received_bytes += (uint32_t)frame->payload_len;

    /* calculate number of dropped packets */
    int extended_max = p->stats.cycles + p->stats.max_seq;
    int expected     = extended_max - p->stats.base_seq + 1;

    int dropped = expected - p->stats.received_pkts;
    p->stats.dropped_pkts = dropped >= 0 ? dropped : 0;

    uint64_t arrival =
        p->stats.initial_rtp +
        + uvgrtp::clock::ntp::diff_now(p->stats.initial_ntp)
        * (p->stats.clock_rate
        / 1000);

	/* calculate interarrival jitter. See RFC 3550 A.8 */
    uint64_t transit = arrival - frame->header.timestamp;

    if (transit > UINT32_MAX)
    {
        transit = UINT32_MAX;
    }

    uint32_t transit32 = (uint32_t)transit;
    uint32_t trans_difference = std::abs((int)(transit32 - p->stats.transit));

    p->stats.transit = transit32;
    p->stats.jitter += (uint32_t)((1.f / 16.f) * ((double)trans_difference - p->stats.jitter));
}

/* RTCP packet handler is responsible for doing two things:
 *
 * - it checks whether the packet is coming from an existing user and if so,
 *   updates that user's session statistics. If the packet is coming from a user,
 *   the user is put on probation where they will stay until enough valid packets
 *   have been received.
 * - it keeps track of participants' SSRCs and if a collision
 *   is detected, the RTP context is updated */
rtp_error_t uvgrtp::rtcp::recv_packet_handler(void *arg, int flags, frame::rtp_frame **out)
{
    (void)flags;

    rtp_error_t ret;
    uvgrtp::frame::rtp_frame *frame = *out;
    uvgrtp::rtcp *rtcp              = (uvgrtp::rtcp *)arg;

    /* If this is the first packet from remote, move the participant from initial_participants_
     * to participants_, initialize its state and put it on probation until enough valid
     * packets from them have been received
     *
     * Otherwise update and monitor the received sequence numbers to determine whether something
     * has gone awry with the sender's sequence number calculations/delivery of packets */
    if (!rtcp->is_participant(frame->header.ssrc)) {
        if ((ret = rtcp->init_new_participant(frame)) != RTP_OK)
            return RTP_GENERIC_ERROR;
    } else if (rtcp->update_participant_seq(frame->header.ssrc, frame->header.seq) != RTP_OK) {
        return RTP_GENERIC_ERROR;
    }

    /* Finally update the jitter/transit/received/dropped bytes/pkts statistics */
    rtcp->update_session_statistics(frame);

    /* Even though RTCP collects information from the packet, this is not the packet's final destination.
     * Thus return RTP_PKT_NOT_HANDLED to indicate that the packet should be passed on to other handlers */
    return RTP_PKT_NOT_HANDLED;
}

rtp_error_t uvgrtp::rtcp::send_packet_handler_vec(void *arg, uvgrtp::buf_vec& buffers)
{
    ssize_t pkt_size = -uvgrtp::frame::HEADER_SIZE_RTP;

    for (auto& buffer : buffers)
        pkt_size += buffer.first;

    if (pkt_size < 0)
        return RTP_INVALID_VALUE;

    return ((uvgrtp::rtcp *)arg)->update_sender_stats(pkt_size);
}

rtp_error_t uvgrtp::rtcp::handle_incoming_packet(uint8_t *buffer, size_t size)
{
    (void)size;

    if (size < RTCP_HEADER_LENGTH)
    {
        LOG_ERROR("Didn't get enough data for an rtcp header");
        return RTP_INVALID_VALUE;
    }

    uvgrtp::frame::rtcp_header header;
    read_rtcp_header(buffer, header);

    if (size < header.length)
    {
        LOG_ERROR("Received partial rtcp packet. Not supported");
        return RTP_NOT_SUPPORTED;
    }

    if (header.version != 0x2) {
        LOG_ERROR("Invalid header version (%u)", header.version);
        return RTP_INVALID_VALUE;
    }

    if (header.padding) {
        LOG_ERROR("Cannot handle padded packets!");
        return RTP_INVALID_VALUE;
    }

    if (header.pkt_type > uvgrtp::frame::RTCP_FT_APP ||
        header.pkt_type < uvgrtp::frame::RTCP_FT_SR) {
        LOG_ERROR("Invalid packet type (%u)!", header.pkt_type);
        return RTP_INVALID_VALUE;
    }

    update_rtcp_bandwidth(size);

    rtp_error_t ret = RTP_INVALID_VALUE;

    switch (header.pkt_type) {
        case uvgrtp::frame::RTCP_FT_SR:
            ret = handle_sender_report_packet(buffer, size, header);
            break;

        case uvgrtp::frame::RTCP_FT_RR:
            ret = handle_receiver_report_packet(buffer, size, header);
            break;

        case uvgrtp::frame::RTCP_FT_SDES:
            ret = handle_sdes_packet(buffer, size, header);
            break;

        case uvgrtp::frame::RTCP_FT_BYE:
            ret = handle_bye_packet(buffer, size);
            break;

        case uvgrtp::frame::RTCP_FT_APP:
            ret = handle_app_packet(buffer, size, header);
            break;

        default:
            LOG_WARN("Unknown packet received, type %d", header.pkt_type);
            break;
    }

    return ret;
}

rtp_error_t uvgrtp::rtcp::handle_sdes_packet(uint8_t* packet, size_t size,
    uvgrtp::frame::rtcp_header& header)
{
    if (!packet || !size)
        return RTP_INVALID_VALUE;

    auto frame = new uvgrtp::frame::rtcp_sdes_packet;
    frame->header = header;
    frame->ssrc = ntohl(*(uint32_t*)&packet[4]);

    auto ret = RTP_OK;
    if (srtcp_ && (ret = srtcp_->handle_rtcp_decryption(flags_, frame->ssrc, packet, size)) != RTP_OK)
        return ret;

    /* Deallocate previous frame from the buffer if it exists, it's going to get overwritten */
    if (participants_[frame->ssrc]->sdes_frame) {
        for (auto& item : participants_[frame->ssrc]->sdes_frame->items)
            delete[](uint8_t*)item.data;
        delete participants_[frame->ssrc]->sdes_frame;
    }

    for (int ptr = 8; ptr < frame->header.length; ) {
        uvgrtp::frame::rtcp_sdes_item item;

        item.type = packet[ptr++];
        item.length = packet[ptr++];
        item.data = (void*)new uint8_t[item.length];

        memcpy(item.data, &packet[ptr], item.length);
        ptr += item.length;
    }

    if (sdes_hook_)
        sdes_hook_(frame);
    else if (sdes_hook_f_)
        sdes_hook_f_(std::shared_ptr<uvgrtp::frame::rtcp_sdes_packet>(frame));
    else
        participants_[frame->ssrc]->sdes_frame = frame;

    return RTP_OK;
}

rtp_error_t uvgrtp::rtcp::handle_bye_packet(uint8_t* packet, size_t size)
{
    if (!packet || !size)
        return RTP_INVALID_VALUE;

    for (size_t i = 4; i < size; i += sizeof(uint32_t)) {
        uint32_t ssrc = ntohl(*(uint32_t*)&packet[i]);

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

rtp_error_t uvgrtp::rtcp::handle_app_packet(uint8_t* packet, size_t size,
    uvgrtp::frame::rtcp_header& header)
{
    if (!packet || !size)
        return RTP_INVALID_VALUE;

    auto frame = new uvgrtp::frame::rtcp_app_packet;
    frame->header = header;
    frame->ssrc = ntohl(*(uint32_t*)&packet[4]);

    auto ret = RTP_OK;
    if (srtcp_ && (ret = srtcp_->handle_rtcp_decryption(flags_, frame->ssrc, packet, size)) != RTP_OK)
        return ret;

    /* Deallocate previous frame from the buffer if it exists, it's going to get overwritten */
    if (!is_participant(frame->ssrc)) {
        LOG_WARN("Got an APP packet from an unknown participant");
        add_participant(frame->ssrc);
    }

    if (participants_[frame->ssrc]->app_frame) {
        delete[] participants_[frame->ssrc]->app_frame->payload;
        delete   participants_[frame->ssrc]->app_frame;
    }
    frame->payload = new uint8_t[frame->header.length];

    memcpy(frame->name, &packet[RTCP_HEADER_SIZE + SSRC_CSRC_SIZE], APP_NAME_SIZE);
    memcpy(frame->payload, &packet[RTCP_HEADER_SIZE + SSRC_CSRC_SIZE + APP_NAME_SIZE],
        frame->header.length - RTCP_HEADER_SIZE + SSRC_CSRC_SIZE + APP_NAME_SIZE);

    if (app_hook_)
        app_hook_(frame);
    else if (app_hook_f_)
        app_hook_f_(std::shared_ptr<uvgrtp::frame::rtcp_app_packet>(frame));
    else
        participants_[frame->ssrc]->app_frame = frame;

    return RTP_OK;
}

rtp_error_t uvgrtp::rtcp::handle_receiver_report_packet(uint8_t* packet, size_t size, 
    uvgrtp::frame::rtcp_header& header)
{
    if (!packet || !size)
        return RTP_INVALID_VALUE;

    auto frame = new uvgrtp::frame::rtcp_receiver_report;
    frame->header = header;
    frame->ssrc = ntohl(*(uint32_t*)&packet[RTCP_HEADER_SIZE]);

    auto ret = RTP_OK;
    if (srtcp_ && (ret = srtcp_->handle_rtcp_decryption(flags_, frame->ssrc, packet, size)) != RTP_OK)
        return ret;

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
    if (participants_[frame->ssrc]->rr_frame)
        delete participants_[frame->ssrc]->rr_frame;

    read_reports(packet, size, frame->header.count, frame->report_blocks);

    if (receiver_hook_)
        receiver_hook_(frame);
    else if (rr_hook_f_)
        rr_hook_f_(std::shared_ptr<uvgrtp::frame::rtcp_receiver_report>(frame));
    else
        participants_[frame->ssrc]->rr_frame = frame;

    return RTP_OK;
}

rtp_error_t uvgrtp::rtcp::handle_sender_report_packet(uint8_t* packet, size_t size,
    uvgrtp::frame::rtcp_header& header)
{
    if (!packet || !size)
        return RTP_INVALID_VALUE;

    auto frame = new uvgrtp::frame::rtcp_sender_report;
    frame->header = header;
    frame->ssrc = ntohl(*(uint32_t*)&packet[4]);

    auto ret = RTP_OK;
    if (srtcp_ && (ret = srtcp_->handle_rtcp_decryption(flags_, frame->ssrc, packet, size)) != RTP_OK)
        return ret;

    if (!is_participant(frame->ssrc)) {
        LOG_WARN("Sender Report received from an unknown participant");
        add_participant(frame->ssrc);
    }

    /* Deallocate previous frame from the buffer if it exists, it's going to get overwritten */
    if (participants_[frame->ssrc]->sr_frame)
        delete participants_[frame->ssrc]->sr_frame;

    frame->sender_info.ntp_msw =    ntohl(*(uint32_t*)&packet[8]);
    frame->sender_info.ntp_lsw =    ntohl(*(uint32_t*)&packet[12]);
    frame->sender_info.rtp_ts =     ntohl(*(uint32_t*)&packet[16]);
    frame->sender_info.pkt_cnt =    ntohl(*(uint32_t*)&packet[20]);
    frame->sender_info.byte_cnt =   ntohl(*(uint32_t*)&packet[24]);

    participants_[frame->ssrc]->stats.sr_ts = uvgrtp::clock::hrc::now();
    participants_[frame->ssrc]->stats.lsr =
        ((frame->sender_info.ntp_msw >> 16) & 0xffff) |
        ((frame->sender_info.ntp_lsw & 0xffff0000) >> 16);

    read_reports(packet, size, frame->header.count, frame->report_blocks);

    if (sender_hook_)
        sender_hook_(frame);
    else if (sr_hook_f_)
        sr_hook_f_(std::shared_ptr<uvgrtp::frame::rtcp_sender_report>(frame));
    else
        participants_[frame->ssrc]->sr_frame = frame;

    return RTP_OK;
}

rtp_error_t uvgrtp::rtcp::construct_rtcp_header(size_t packet_size, 
    uint8_t*& frame,
    uint16_t secondField, 
    uvgrtp::frame::RTCP_FRAME_TYPE frame_type, 
    bool add_local_ssrc
)
{
    if (packet_size > UINT16_MAX)
    {
        LOG_ERROR("RTCP receiver report packet size too large!");
        return RTP_GENERIC_ERROR;
    }

    frame = new uint8_t[packet_size];
    memset(frame, 0, packet_size);

    // header |V=2|P|    SC   |  PT  |             length            |
    frame[0] = (2 << 6) | (0 << 5) | secondField;
    frame[1] = frame_type;

    // TODO: This should be size in 32-bit words - 1
    *(uint16_t*)&frame[2] = htons((u_short)packet_size);

    if (add_local_ssrc)
    {
        *(uint32_t*)&frame[RTCP_HEADER_SIZE] = htonl(ssrc_);
    }

    return RTP_OK;
}

void uvgrtp::rtcp::read_rtcp_header(uint8_t* packet, uvgrtp::frame::rtcp_header& header)
{
    header.version = (packet[0] >> 6) & 0x3;
    header.padding = (packet[0] >> 5) & 0x1;

    header.pkt_type = packet[1] & 0xff;

    if (header.pkt_type == uvgrtp::frame::RTCP_FT_APP)
        header.pkt_subtype = packet[0] & 0x1f;
    else
        header.count = packet[0] & 0x1f;

    header.length = ntohs(*(uint16_t*)&packet[2]);
}

void uvgrtp::rtcp::read_reports(uint8_t* packet, size_t size, uint8_t count, std::vector<uvgrtp::frame::rtcp_report_block>& reports)
{
    uint32_t report_section = RTCP_HEADER_SIZE + SSRC_CSRC_SIZE + SENDER_INFO_SIZE;

    for (int i = 0; i < count; ++i) {
        uint32_t report_position = report_section + (i * REPORT_BLOCK_SIZE);

        if (size >= report_position + REPORT_BLOCK_SIZE)
        {
            uvgrtp::frame::rtcp_report_block report;
            report.ssrc =       ntohl(*(uint32_t*)&packet[report_position + 0]);
            report.fraction =  (ntohl(*(uint32_t*)&packet[report_position + 4])) >> 24;
            report.lost =      (ntohl(*(int32_t*)&packet[report_position + 4])) & 0xfffffd;
            report.last_seq =   ntohl(*(uint32_t*)&packet[report_position + 8]);
            report.jitter =     ntohl(*(uint32_t*)&packet[report_position + 12]);
            report.lsr =        ntohl(*(uint32_t*)&packet[report_position + 16]);
            report.dlsr =       ntohl(*(uint32_t*)&packet[report_position + 20]);

            reports.push_back(report);
        }
        else
        {
            LOG_DEBUG("Received rtcp packet is smaller than the indicated number of reports!");
        }
    }
}

rtp_error_t uvgrtp::rtcp::send_rtcp_packet_to_participants(uint8_t* frame, size_t frame_size)
{
    rtp_error_t ret = RTP_OK;
    for (auto& p : participants_) {
        if ((ret = p.second->socket->sendto(p.second->address, frame, frame_size, 0)) != RTP_OK) {
            LOG_ERROR("Sending rtcp packet with sendto() failed!");
            delete[] frame;
            break;
        }

        update_rtcp_bandwidth(frame_size);
    }

    return ret;
}

rtp_error_t uvgrtp::rtcp::generate_report()
{
    rtcp_pkt_sent_count_++;

    if (!senders_ && our_role_ == SENDER) {
        LOG_DEBUG("Session does not have any RTP senders!");
        return RTP_NOT_READY;
    }

    rtp_error_t ret = RTP_OK;
    uint8_t* frame = nullptr;
    int ptr = RTCP_HEADER_SIZE + SSRC_CSRC_SIZE;

    size_t frame_size = RTCP_HEADER_SIZE + SSRC_CSRC_SIZE + (size_t)num_receivers_ * REPORT_BLOCK_SIZE;

    if (flags_ & RCE_SRTP)
        frame_size += UVG_SRTCP_INDEX_LENGTH + UVG_AUTH_TAG_LENGTH;

    if (our_role_ == SENDER)
    {
        // sender reports have sender information in addition compared to receiver reports
        frame_size += SENDER_INFO_SIZE;
        construct_rtcp_header(frame_size, frame, num_receivers_, uvgrtp::frame::RTCP_FT_SR, true);
    }
    else // RECEIVER
    {
        construct_rtcp_header(frame_size, frame, num_receivers_, uvgrtp::frame::RTCP_FT_RR, true);
    }

    if (our_role_ == SENDER)
    {
        if (clock_start_ == 0)
        {
          clock_start_ = uvgrtp::clock::ntp::now();
        }

        /* Sender information */
        uint64_t ntp_ts = uvgrtp::clock::ntp::now();

        // TODO: Shouldn't this be the NTP timestamp of previous RTP packet?
        uint64_t rtp_ts = rtp_ts_start_ + (uvgrtp::clock::ntp::diff(clock_start_, ntp_ts)) * clock_rate_ / 1000;

        SET_NEXT_FIELD_32(frame, ptr, htonl(ntp_ts >> 32));
        SET_NEXT_FIELD_32(frame, ptr, htonl(ntp_ts & 0xffffffff));
        SET_NEXT_FIELD_32(frame, ptr, htonl((u_long)rtp_ts));
        SET_NEXT_FIELD_32(frame, ptr, htonl(our_stats.sent_pkts));
        SET_NEXT_FIELD_32(frame, ptr, htonl(our_stats.sent_bytes));
    }

    // the report blocks for sender or receiver report. Both have same reports.

    for (auto& p : participants_) {
        int dropped = p.second->stats.dropped_pkts;
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

    if (srtcp_ && (ret = srtcp_->handle_rtcp_encryption(flags_, rtcp_pkt_sent_count_, ssrc_, frame, frame_size)) != RTP_OK)
    {
        LOG_DEBUG("Encryption failed. Not sending packet");
        delete[] frame;
        return ret;
    }

    return send_rtcp_packet_to_participants(frame, frame_size);
}

rtp_error_t uvgrtp::rtcp::send_sdes_packet(std::vector<uvgrtp::frame::rtcp_sdes_item>& items)
{
    if (items.empty()) {
        LOG_ERROR("Cannot send an empty SDES packet!");
        return RTP_INVALID_VALUE;
    }

    if (num_receivers_ > MAX_SUPPORTED_PARTICIPANTS) {
        LOG_ERROR("Source count is larger than packet supports!");

        // TODO: Multiple SDES packets should be sent in this case
        return RTP_GENERIC_ERROR;
    }

    uint8_t* frame = nullptr;
    rtp_error_t ret = RTP_OK;
    size_t frame_size = 0;

    // TODO: This does not seem correct. Each SDES item has its own SSRC/CSRC 
    // and there is no one SSRC per packet. This also does not take into account the size of payload
    frame_size = RTCP_HEADER_SIZE + SSRC_CSRC_SIZE;
    frame_size += items.size() * 2; /* sdes item type + length */
    int ptr = RTCP_HEADER_SIZE + SSRC_CSRC_SIZE;

    for (auto& item : items)
        frame_size += item.length;

    construct_rtcp_header(frame_size, frame, num_receivers_, uvgrtp::frame::RTCP_FT_SDES, true);

    for (auto& item : items) {
        frame[ptr++] = item.type;
        frame[ptr++] = item.length;
        memcpy(frame + ptr, item.data, item.length);
        ptr += item.length;
    }

    if (srtcp_ && (ret = srtcp_->handle_rtcp_encryption(flags_, rtcp_pkt_sent_count_, ssrc_, frame, frame_size)) != RTP_OK)
    {
        delete[] frame;
        return ret;
    }

    return send_rtcp_packet_to_participants(frame, frame_size);
}

rtp_error_t uvgrtp::rtcp::send_bye_packet(std::vector<uint32_t> ssrcs)
{
    if (ssrcs.empty()) {
        LOG_WARN("Source Count in RTCP BYE packet is 0");
    }

    size_t frame_size = RTCP_HEADER_SIZE + ssrcs.size() * SSRC_CSRC_SIZE;
    uint8_t* frame = nullptr;
    int ptr = RTCP_HEADER_SIZE;

    construct_rtcp_header(frame_size, frame, (ssrcs.size() & 0x1f), uvgrtp::frame::RTCP_FT_BYE, false);

    for (auto& ssrc : ssrcs)
        SET_NEXT_FIELD_32(frame, ptr, htonl(ssrc));

    return send_rtcp_packet_to_participants(frame, frame_size);
}

rtp_error_t uvgrtp::rtcp::send_app_packet(char* name, uint8_t subtype,
    size_t payload_len, uint8_t* payload)
{
    rtp_error_t ret = RTP_OK;
    uint8_t* frame = nullptr;
    size_t frame_size = RTCP_HEADER_SIZE + SSRC_CSRC_SIZE + APP_NAME_SIZE + payload_len;

    construct_rtcp_header(frame_size, frame, (subtype & 0x1f), uvgrtp::frame::RTCP_FT_APP, true);

    memcpy(&frame[RTCP_HEADER_SIZE + SSRC_CSRC_SIZE], name, APP_NAME_SIZE);
    memcpy(&frame[RTCP_HEADER_SIZE + SSRC_CSRC_SIZE + APP_NAME_SIZE], payload, payload_len);

    if (srtcp_ && (ret = srtcp_->handle_rtcp_encryption(flags_, rtcp_pkt_sent_count_, ssrc_, frame, frame_size)) != RTP_OK)
    {
        delete[] frame;
        return ret;
    }

    return send_rtcp_packet_to_participants(frame, frame_size);
}

