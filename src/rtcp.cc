#include "uvgrtp/rtcp.hh"

#include "hostname.hh"
#include "poll.hh"
#include "rtp.hh"
#include "srtp/srtcp.hh"
#include "rtcp_packets.hh"

#include "uvgrtp/debug.hh"
#include "uvgrtp/util.hh"
#include "uvgrtp/frame.hh"

#ifndef _WIN32
#include <sys/time.h>
#endif

#include <cassert>
#include <cstdlib>
#include <cstring>
#include <iostream>


/* TODO: explain these constants */
const uint32_t RTP_SEQ_MOD    = 1 << 16;
const uint32_t MIN_SEQUENTIAL = 2;
const uint32_t MAX_DROPOUT    = 3000;
const uint32_t MAX_MISORDER   = 100;
const uint32_t DEFAULT_RTCP_INTERVAL_MS = 5000;

constexpr int ESTIMATED_MAX_RECEPTION_TIME_MS = 10;

const uint32_t MAX_SUPPORTED_PARTICIPANTS = 31;

uvgrtp::rtcp::rtcp(std::shared_ptr<uvgrtp::rtp> rtp, std::string cname, int flags):
    flags_(flags), our_role_(RECEIVER),
    tp_(0), tc_(0), tn_(0), pmembers_(0),
    members_(0), senders_(0), rtcp_bandwidth_(0),
    we_sent_(false), avg_rtcp_pkt_pize_(0), rtcp_pkt_count_(0),
    rtcp_pkt_sent_count_(0), initial_(true), ssrc_(rtp->get_ssrc()), 
    num_receivers_(0),
    sender_hook_(nullptr),
    receiver_hook_(nullptr),
    sdes_hook_(nullptr),
    app_hook_(nullptr),
    sr_hook_f_(nullptr),
    sr_hook_u_(nullptr),
    rr_hook_f_(nullptr),
    rr_hook_u_(nullptr),
    sdes_hook_f_(nullptr),
    sdes_hook_u_(nullptr),
    app_hook_f_(nullptr),
    app_hook_u_(nullptr),
    active_(false),
    interval_ms_(DEFAULT_RTCP_INTERVAL_MS)
{
    clock_rate_   = rtp->get_clock_rate();

    clock_start_  = 0;
    rtp_ts_start_ = 0;

    report_generator_   = nullptr;

    srtcp_        = nullptr;

    zero_stats(&our_stats);
}

uvgrtp::rtcp::rtcp(std::shared_ptr<uvgrtp::rtp> rtp, std::string cname, 
    std::shared_ptr<uvgrtp::srtcp> srtcp, int flags):
    rtcp(rtp, cname, flags)
{
    srtcp_ = srtcp;
}

uvgrtp::rtcp::~rtcp()
{
    if (active_)
    {
        stop();
    }
}

void uvgrtp::rtcp::free_participant(rtcp_participant* participant)
{
    participant->socket = nullptr;

    if (participant->sr_frame)
    {
        delete participant->sr_frame;
    }
    if (participant->rr_frame)
    {
        delete participant->rr_frame;
    }
    if (participant->sdes_frame)
    {
        delete participant->sdes_frame;
    }
    if (participant->app_frame)
    {
        delete participant->app_frame;
    }

    delete participant;
}

rtp_error_t uvgrtp::rtcp::start()
{
    if (sockets_.empty())
    {
        LOG_ERROR("Cannot start RTCP Runner because no connections have been initialized");
        return RTP_INVALID_VALUE;
    }
    active_ = true;

    report_generator_.reset(new std::thread(rtcp_runner, this, interval_ms_));

    return RTP_OK;
}

rtp_error_t uvgrtp::rtcp::stop()
{
    // TODO: Make thread safe. I think this kind of works, but not in a flexible way
    if (!active_)
    {
        /* free all receiver statistic structs */
        for (auto& participant : participants_)
        {
            free_participant(participant.second);
        }
        participants_.clear();

        for (auto& participant : initial_participants_)
        {
            free_participant(participant);
        }
        initial_participants_.clear();

        return RTP_OK;
    }

    active_ = false;

    if (report_generator_ && report_generator_->joinable())
    {
        report_generator_->join();
    }

    /* when the member count is less than 50,
     * we can just send the BYE message and destroy the session */
    if (members_ >= 50)
    {
        tp_       = tc_;
        members_  = 1;
        pmembers_ = 1;
        initial_  = true;
        we_sent_  = false;
        senders_  = 0;
    }

    /* Send BYE packet with our SSRC to all participants */
    return uvgrtp::rtcp::send_bye_packet({ ssrc_ });
}

void uvgrtp::rtcp::rtcp_runner(rtcp* rtcp, int interval)
{
    LOG_INFO("RTCP instance created! RTCP interval: %i ms", interval);

    // RFC 3550 says to wait half interval before sending first report
    int initial_sleep_ms = interval / 2;
    LOG_DEBUG("Sleeping for %i ms before sending first RTCP report", initial_sleep_ms);
    std::this_thread::sleep_for(std::chrono::milliseconds(initial_sleep_ms));

    uint8_t buffer[MAX_PACKET];

    uvgrtp::clock::hrc::hrc_t start = uvgrtp::clock::hrc::now();

    int i = 0;
    while (rtcp->is_active())
    {
        long int next_sendslot = i * interval;
        long int run_time = uvgrtp::clock::hrc::diff_now(start);
        long int diff_ms = next_sendslot - run_time;

        if (diff_ms <= 0)
        {
            ++i;

            LOG_DEBUG("Sending RTCP report number %i at time slot %i ms", i, next_sendslot);

            rtp_error_t ret = RTP_OK;
            if ((ret = rtcp->generate_report()) != RTP_OK && ret != RTP_NOT_READY)
            {
                LOG_ERROR("Failed to send RTCP status report!");
            }
        } else if (diff_ms > ESTIMATED_MAX_RECEPTION_TIME_MS) { // try receiving if we have time
            // Receive RTCP reports until time to send report
            int nread = 0;

            int poll_timout = diff_ms - ESTIMATED_MAX_RECEPTION_TIME_MS;

            // using max poll we make sure that exiting uvgRTP doesn't take several seconds
            int max_poll_timeout_ms = 100;
            if (poll_timout > max_poll_timeout_ms)
            {
                poll_timout = max_poll_timeout_ms;
            }

            rtp_error_t ret = uvgrtp::poll::poll(rtcp->get_sockets(), buffer, MAX_PACKET,
                                                 poll_timout, &nread);

            if (ret == RTP_OK && nread > 0)
            {
                (void)rtcp->handle_incoming_packet(buffer, (size_t)nread);
            } else if (ret == RTP_INTERRUPTED) {
                /* do nothing */
            } else {
                LOG_ERROR("recvfrom failed, %d", ret);
            }
        } else { // sleep until it is time to send the report
            std::this_thread::sleep_for(std::chrono::milliseconds(diff_ms));
        }
    }
}

rtp_error_t uvgrtp::rtcp::add_participant(std::string dst_addr, uint16_t dst_port, uint16_t src_port, uint32_t clock_rate)
{
    if (dst_addr == "" || !dst_port || !src_port)
    {
        LOG_ERROR("Invalid values given (%s, %d, %d), cannot create RTCP instance",
                dst_addr.c_str(), dst_port, src_port);
        return RTP_INVALID_VALUE;
    }

    rtp_error_t ret;
    rtcp_participant *p;

    p = new rtcp_participant();

    zero_stats(&p->stats);

    p->socket = std::shared_ptr<uvgrtp::socket> (new uvgrtp::socket(0));

    if ((ret = p->socket->init(AF_INET, SOCK_DGRAM, 0)) != RTP_OK)
    {
        return ret;
    }

    int enable = 1;

    if ((ret = p->socket->setsockopt(SOL_SOCKET, SO_REUSEADDR, (const char *)&enable, sizeof(int))) != RTP_OK)
    {
        return ret;
    }

#ifdef _WIN32
    /* Make the socket non-blocking */
    int enabled = 1;

    if (::ioctlsocket(p->socket->get_raw_socket(), FIONBIO, (u_long *)&enabled) < 0)
    {
        LOG_ERROR("Failed to make the socket non-blocking!");
    }
#endif

    /* Set read timeout (5s for now)
     *
     * This means that the socket is listened for 5s at a time and after the timeout,
     * Send Report is sent to all participants */
    struct timeval tv;
    tv.tv_sec = 3;
    tv.tv_usec = 0;

    if ((ret = p->socket->setsockopt(SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv))) != RTP_OK)
    {
        return ret;
    }

    LOG_WARN("Binding to port %d (source port)", src_port);

    if ((ret = p->socket->bind(AF_INET, INADDR_ANY, src_port)) != RTP_OK)
    {
        return ret;
    }

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
        // TODO: Support more participants by sending multiple messages at the same time
        return RTP_GENERIC_ERROR;
    }

    /* RTCP is not in use for this media stream,
     * create a "fake" participant that is only used for storing statistics information */
    if (initial_participants_.empty())
    {
        participants_[ssrc] = new rtcp_participant();
        zero_stats(&participants_[ssrc]->stats);
    } else {
        participants_[ssrc] = initial_participants_.back();
        initial_participants_.pop_back();
    }
    num_receivers_++;

    participants_[ssrc]->rr_frame    = nullptr;
    participants_[ssrc]->sr_frame    = nullptr;
    participants_[ssrc]->sdes_frame  = nullptr;
    participants_[ssrc]->app_frame   = nullptr;

    return RTP_OK;
}

rtp_error_t uvgrtp::rtcp::remove_all_hooks()
{
    sr_mutex_.lock();
    sender_hook_ = nullptr;
    sr_hook_f_   = nullptr;
    sr_hook_u_   = nullptr;
    sr_mutex_.unlock();

    rr_mutex_.lock();
    receiver_hook_ = nullptr;
    rr_hook_f_     = nullptr;
    rr_hook_u_     = nullptr;
    rr_mutex_.unlock();

    sdes_mutex_.lock();
    sdes_hook_   = nullptr;
    sdes_hook_f_ = nullptr;
    sdes_hook_u_ = nullptr;
    sdes_mutex_.unlock();

    app_mutex_.lock();
    app_hook_   = nullptr;
    app_hook_f_ = nullptr;
    app_hook_u_ = nullptr;
    app_mutex_.unlock();
    return RTP_OK;
}

rtp_error_t uvgrtp::rtcp::install_sender_hook(void (*hook)(uvgrtp::frame::rtcp_sender_report*))
{
    if (!hook)
    {
        return RTP_INVALID_VALUE;
    }

    sr_mutex_.lock();
    sender_hook_ = hook;
    sr_hook_f_   = nullptr;
    sr_hook_u_   = nullptr;
    sr_mutex_.unlock();

    return RTP_OK;
}

rtp_error_t uvgrtp::rtcp::install_sender_hook(std::function<void(std::shared_ptr<uvgrtp::frame::rtcp_sender_report>)> sr_handler)
{
    if (!sr_handler)
    {
        return RTP_INVALID_VALUE;
    }

    sr_mutex_.lock();
    sender_hook_ = nullptr;
    sr_hook_f_   = sr_handler;
    sr_hook_u_   = nullptr;
    sr_mutex_.unlock();

    return RTP_OK;
}

rtp_error_t uvgrtp::rtcp::install_sender_hook(std::function<void(std::unique_ptr<uvgrtp::frame::rtcp_sender_report>)> sr_handler)
{
    if (!sr_handler)
    {
        return RTP_INVALID_VALUE;
    }

    sr_mutex_.lock();
    sender_hook_ = nullptr;
    sr_hook_f_   = nullptr;
    sr_hook_u_   = sr_handler;
    sr_mutex_.unlock();

    return RTP_OK;
}

rtp_error_t uvgrtp::rtcp::install_receiver_hook(void (*hook)(uvgrtp::frame::rtcp_receiver_report*))
{
    if (!hook)
    {
        return RTP_INVALID_VALUE;
    }

    rr_mutex_.lock();
    receiver_hook_ = hook;
    rr_hook_f_     = nullptr;
    rr_hook_u_     = nullptr;
    rr_mutex_.unlock();

    return RTP_OK;
}

rtp_error_t uvgrtp::rtcp::install_receiver_hook(std::function<void(std::shared_ptr<uvgrtp::frame::rtcp_receiver_report>)> rr_handler)
{
    if (!rr_handler)
    {
        return RTP_INVALID_VALUE;
    }

    rr_mutex_.lock();
    receiver_hook_ = nullptr;
    rr_hook_f_     = rr_handler;
    rr_hook_u_     = nullptr;
    rr_mutex_.unlock();

    return RTP_OK;
}

rtp_error_t uvgrtp::rtcp::install_receiver_hook(std::function<void(std::unique_ptr<uvgrtp::frame::rtcp_receiver_report>)> rr_handler)
{
    if (!rr_handler)
    {
        return RTP_INVALID_VALUE;
    }

    rr_mutex_.lock();
    receiver_hook_ = nullptr;
    rr_hook_f_     = nullptr;
    rr_hook_u_     = rr_handler;
    rr_mutex_.unlock();

    return RTP_OK;
}

rtp_error_t uvgrtp::rtcp::install_sdes_hook(void (*hook)(uvgrtp::frame::rtcp_sdes_packet*))
{
    if (!hook)
    {
        return RTP_INVALID_VALUE;
    }

    sdes_mutex_.lock();
    sdes_hook_   = hook;
    sdes_hook_f_ = nullptr;
    sdes_hook_u_ = nullptr;
    sdes_mutex_.unlock();

    return RTP_OK;
}

rtp_error_t uvgrtp::rtcp::install_sdes_hook(std::function<void(std::shared_ptr<uvgrtp::frame::rtcp_sdes_packet>)> sdes_handler)
{
    if (!sdes_handler)
    {
        return RTP_INVALID_VALUE;
    }

    sdes_mutex_.lock();
    sdes_hook_   = nullptr;
    sdes_hook_f_ = sdes_handler;
    sdes_hook_u_ = nullptr;
    sdes_mutex_.unlock();

    return RTP_OK;
}

rtp_error_t uvgrtp::rtcp::install_sdes_hook(std::function<void(std::unique_ptr<uvgrtp::frame::rtcp_sdes_packet>)> sdes_handler)
{
    if (!sdes_handler)
    {
        return RTP_INVALID_VALUE;
    }

    sdes_mutex_.lock();
    sdes_hook_   = nullptr;
    sdes_hook_f_ = nullptr;
    sdes_hook_u_ = sdes_handler;
    sdes_mutex_.unlock();

    return RTP_OK;
}

rtp_error_t uvgrtp::rtcp::install_app_hook(void (*hook)(uvgrtp::frame::rtcp_app_packet*))
{
    if (!hook)
    {
        return RTP_INVALID_VALUE;
    }

    app_mutex_.lock();
    app_hook_   = hook;
    app_hook_f_ = nullptr;
    app_hook_u_ = nullptr;
    app_mutex_.unlock();

    return RTP_OK;
}

rtp_error_t uvgrtp::rtcp::install_app_hook(std::function<void(std::shared_ptr<uvgrtp::frame::rtcp_app_packet>)> app_handler)
{
    if (!app_handler)
    {
        return RTP_INVALID_VALUE;
    }

    app_mutex_.lock();
    app_hook_   = nullptr;
    app_hook_f_ = app_handler;
    app_hook_u_ = nullptr;
    app_mutex_.unlock();

    return RTP_OK;
}

rtp_error_t uvgrtp::rtcp::install_app_hook(std::function<void(std::unique_ptr<uvgrtp::frame::rtcp_app_packet>)> app_handler)
{
    if (!app_handler)
    {
        return RTP_INVALID_VALUE;
    }

    app_mutex_.lock();
    app_hook_   = nullptr;
    app_hook_f_ = nullptr;
    app_hook_u_ = app_handler;
    app_mutex_.unlock();

    return RTP_OK;
}

uvgrtp::frame::rtcp_sender_report* uvgrtp::rtcp::get_sender_packet(uint32_t ssrc)
{
    if (participants_.find(ssrc) == participants_.end())
    {
        return nullptr;
    }

    sr_mutex_.lock();
    auto frame = participants_[ssrc]->sr_frame;
    participants_[ssrc]->sr_frame = nullptr;
    sr_mutex_.unlock();

    return frame;
}

uvgrtp::frame::rtcp_receiver_report* uvgrtp::rtcp::get_receiver_packet(uint32_t ssrc)
{
    if (participants_.find(ssrc) == participants_.end())
    {
        return nullptr;
    }

    rr_mutex_.lock();
    auto frame = participants_[ssrc]->rr_frame;
    participants_[ssrc]->rr_frame = nullptr;
    rr_mutex_.unlock();

    return frame;
}

uvgrtp::frame::rtcp_sdes_packet* uvgrtp::rtcp::get_sdes_packet(uint32_t ssrc)
{
    if (participants_.find(ssrc) == participants_.end())
    {
        return nullptr;
    }

    sdes_mutex_.lock();
    auto frame = participants_[ssrc]->sdes_frame;
    participants_[ssrc]->sdes_frame = nullptr;
    sdes_mutex_.unlock();

    return frame;
}

uvgrtp::frame::rtcp_app_packet* uvgrtp::rtcp::get_app_packet(uint32_t ssrc)
{
    if (participants_.find(ssrc) == participants_.end())
    {
        return nullptr;
    }

    app_mutex_.lock();
    auto frame = participants_[ssrc]->app_frame;
    participants_[ssrc]->app_frame = nullptr;
    app_mutex_.unlock();

    return frame;
}

std::vector<uvgrtp::socket>& uvgrtp::rtcp::get_sockets()
{
    return sockets_;
}

std::vector<uint32_t> uvgrtp::rtcp::get_participants() const
{
    std::vector<uint32_t> ssrcs;

    for (auto& i : participants_)
    {
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


void uvgrtp::rtcp::zero_stats(uvgrtp::sender_statistics *stats)
{
    stats->sent_pkts  = 0;
    stats->sent_bytes = 0;

    stats->sent_rtp_packet = false;
}

void uvgrtp::rtcp::zero_stats(uvgrtp::receiver_statistics *stats)
{
    stats->received_pkts  = 0;
    stats->dropped_pkts   = 0;
    stats->received_bytes = 0;

    stats->received_rtp_packet = false;

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

bool uvgrtp::rtcp::is_participant(uint32_t ssrc) const
{
    return participants_.find(ssrc) != participants_.end();
}

void uvgrtp::rtcp::set_ts_info(uint64_t clock_start, uint32_t clock_rate, uint32_t rtp_ts_start)
{
    clock_start_  = clock_start;
    clock_rate_   = clock_rate;
    rtp_ts_start_ = rtp_ts_start;
}

void uvgrtp::rtcp::sender_update_stats(const uvgrtp::frame::rtp_frame *frame)
{
    if (!frame)
    {
        return;
    }

    if (frame->payload_len > UINT32_MAX)
    {
        LOG_ERROR("Payload size larger than uint32 max which is not supported by RFC 3550");
        return;
    }

    our_stats.sent_pkts  += 1;
    our_stats.sent_bytes += (uint32_t)frame->payload_len;
    our_stats.sent_rtp_packet = true;
}

rtp_error_t uvgrtp::rtcp::init_new_participant(const uvgrtp::frame::rtp_frame *frame)
{
    rtp_error_t ret;

    if ((ret = uvgrtp::rtcp::add_participant(frame->header.ssrc)) != RTP_OK)
    {
        return ret;
    }

    if ((ret = uvgrtp::rtcp::init_participant_seq(frame->header.ssrc, frame->header.seq)) != RTP_OK)
    {
        return ret;
    }

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
    {
        our_role_ = SENDER;
    }

    if (our_stats.sent_bytes + pkt_size > UINT32_MAX)
    {
        LOG_ERROR("Sent bytes overflow");
    }

    our_stats.sent_pkts  += 1;
    our_stats.sent_bytes += (uint32_t)pkt_size;
    our_stats.sent_rtp_packet = true;

    return RTP_OK;
}

rtp_error_t uvgrtp::rtcp::init_participant_seq(uint32_t ssrc, uint16_t base_seq)
{
    if (participants_.find(ssrc) == participants_.end())
    {
        return RTP_NOT_FOUND;
    }

    participants_[ssrc]->stats.base_seq = base_seq;
    participants_[ssrc]->stats.max_seq  = base_seq;
    participants_[ssrc]->stats.bad_seq  = (RTP_SEQ_MOD + 1)%UINT32_MAX;

    return RTP_OK;
}

rtp_error_t uvgrtp::rtcp::update_participant_seq(uint32_t ssrc, uint16_t seq)
{
    if (participants_.find(ssrc) == participants_.end())
    {
        LOG_ERROR("Did not find participant SSRC when updating seq");
        return RTP_GENERIC_ERROR;
    }

    auto p = participants_[ssrc];
    uint16_t udelta = seq - p->stats.max_seq;

    /* Source is not valid until MIN_SEQUENTIAL packets with
    * sequential sequence numbers have been received.  */
    if (p->probation)
    {
       /* packet is in sequence */
       if (seq == p->stats.max_seq + 1)
       {
           p->probation--;
           p->stats.max_seq = seq;
           if (!p->probation)
           {
               uvgrtp::rtcp::init_participant_seq(ssrc, seq);
               return RTP_OK;
           }
       } else {
           p->probation = MIN_SEQUENTIAL - 1;
           p->stats.max_seq = seq;
       }

       return RTP_NOT_READY;
    } else if (udelta < MAX_DROPOUT) {
       /* in order, with permissible gap */
       if (seq < p->stats.max_seq)
       {
           /* Sequence number wrapped - count another 64K cycle.  */
           p->stats.cycles += 1;
       }
       p->stats.max_seq = seq;
    } else if (udelta <= RTP_SEQ_MOD - MAX_MISORDER) {
       /* the sequence number made a very large jump */
       if (seq == p->stats.bad_seq)
       {
           /* Two sequential packets -- assume that the other side
            * restarted without telling us so just re-sync
            * (i.e., pretend this was the first packet).  */
           uvgrtp::rtcp::init_participant_seq(ssrc, seq);
       } else {
           p->stats.bad_seq = (seq + 1) & (RTP_SEQ_MOD - 1);
           LOG_ERROR("Invalid sequence number. Seq jump: %u -> %u", p->stats.max_seq, seq);
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
    {
        return RTP_SSRC_COLLISION;
    }

    zero_stats(&our_stats);

    return RTP_OK;
}

bool uvgrtp::rtcp::collision_detected(uint32_t ssrc, const sockaddr_in& src_addr) const
{
    if (participants_.find(ssrc) == participants_.end())
    {
        return false;
    }

    auto sender = participants_.at(ssrc);

    if (src_addr.sin_port        != sender->address.sin_port &&
        src_addr.sin_addr.s_addr != sender->address.sin_addr.s_addr)
    {
        return true;
    }

    return false;
}

void uvgrtp::rtcp::update_session_statistics(const uvgrtp::frame::rtp_frame *frame)
{
    auto p = participants_[frame->header.ssrc];

    p->stats.received_rtp_packet = true;

    p->stats.received_pkts  += 1;
    p->stats.received_bytes += (uint32_t)frame->payload_len;

    /* calculate number of dropped packets */
    int extended_max = p->stats.cycles + p->stats.max_seq;
    int expected     = extended_max - p->stats.base_seq + 1;

    int dropped = expected - p->stats.received_pkts;
    p->stats.dropped_pkts = dropped >= 0 ? dropped : 0;

    // the arrival time expressed as an RTP timestamp
    uint32_t arrival =
        p->stats.initial_rtp +
        + (uint32_t)uvgrtp::clock::ntp::diff_now(p->stats.initial_ntp)*(p->stats.clock_rate / 1000);

    // calculate interarrival jitter. See RFC 3550 A.8
    uint32_t transit = arrival - frame->header.timestamp; // A.8: int transit = arrival - r->ts
    uint32_t trans_difference = std::abs((int)(transit - p->stats.transit));

    // update statistics
    p->stats.transit = transit;
    p->stats.jitter += (1.f / 16.f) * ((double)trans_difference - p->stats.jitter);
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

    // The validity of the header has been checked by previous handlers

    uvgrtp::frame::rtp_frame *frame = *out;
    uvgrtp::rtcp *rtcp              = (uvgrtp::rtcp *)arg;

    /* If this is the first packet from remote, move the participant from initial_participants_
     * to participants_, initialize its state and put it on probation until enough valid
     * packets from them have been received
     *
     * Otherwise update and monitor the received sequence numbers to determine whether something
     * has gone awry with the sender's sequence number calculations/delivery of packets */
    rtp_error_t ret = RTP_OK;
    if (!rtcp->is_participant(frame->header.ssrc))
    {
        if ((rtcp->init_new_participant(frame)) != RTP_OK)
        {
            LOG_ERROR("Failed to initiate new participant");
            return RTP_GENERIC_ERROR;
        }
    } else if ((ret = rtcp->update_participant_seq(frame->header.ssrc, frame->header.seq)) != RTP_OK) {
        if (ret == RTP_NOT_READY) {
            return RTP_OK;
        }
        else {
            LOG_ERROR("Failed to update participant with seq %u", frame->header.seq);
            return ret;
        }
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
    {
        pkt_size += buffer.first;
    }

    if (pkt_size < 0)
    {
        return RTP_INVALID_VALUE;
    }

    return ((uvgrtp::rtcp *)arg)->update_sender_stats(pkt_size);
}

rtp_error_t uvgrtp::rtcp::handle_incoming_packet(uint8_t *buffer, size_t size)
{
    size_t ptr = 0;
    size_t remaining_size = size;

    int packets = 0;

    // this handles each separate rtcp packet in a compound packet
    while (remaining_size > 0)
    {
        ++packets;
        if (remaining_size < RTCP_HEADER_LENGTH)
        {
            LOG_ERROR("Didn't get enough data for an rtcp header. Packet # %i Got data: %lli", packets, remaining_size);
            return RTP_INVALID_VALUE;
        }

        uint8_t* packet_location = buffer + ptr;

        uvgrtp::frame::rtcp_header header;
        read_rtcp_header(packet_location, header);

        // the length field is the rtcp packet size measured in 32-bit words - 1
        uint32_t size_of_rtcp_packet = (header.length + 1) * sizeof(uint32_t);

        if (remaining_size < size_of_rtcp_packet)
        {
            LOG_ERROR("Received a partial rtcp packet. Not supported!");
            return RTP_NOT_SUPPORTED;
        }

        if (header.version != 0x2)
        {
            LOG_ERROR("Invalid header version (%u)", header.version);
            return RTP_INVALID_VALUE;
        }

        if (header.padding)
        {
            LOG_ERROR("Cannot handle padded packets!");
            return RTP_INVALID_VALUE;
        }

        if (header.pkt_type > uvgrtp::frame::RTCP_FT_APP ||
            header.pkt_type < uvgrtp::frame::RTCP_FT_SR)
        {
            LOG_ERROR("Invalid packet type (%u)!", header.pkt_type);
            return RTP_INVALID_VALUE;
        }

        update_rtcp_bandwidth(size_of_rtcp_packet);

        rtp_error_t ret = RTP_INVALID_VALUE;

        switch (header.pkt_type)
        {
            case uvgrtp::frame::RTCP_FT_SR:
                ret = handle_sender_report_packet(packet_location, size_of_rtcp_packet, header);
                break;

            case uvgrtp::frame::RTCP_FT_RR:
                ret = handle_receiver_report_packet(packet_location, size_of_rtcp_packet, header);
                break;

            case uvgrtp::frame::RTCP_FT_SDES:
                ret = handle_sdes_packet(packet_location, size_of_rtcp_packet, header);
                break;

            case uvgrtp::frame::RTCP_FT_BYE:
                ret = handle_bye_packet(packet_location, size_of_rtcp_packet);
                break;

            case uvgrtp::frame::RTCP_FT_APP:
                ret = handle_app_packet(packet_location, size_of_rtcp_packet, header);
                break;

            default:
                LOG_WARN("Unknown packet received, type %d", header.pkt_type);
                break;
        }

        if (ret != RTP_OK)
        {
            return ret;
        }

        ptr += size_of_rtcp_packet;
        remaining_size -= size_of_rtcp_packet;
    }

    if (packets > 1)
    {
        LOG_DEBUG("Received a compound RTCP frame with %i packets", packets);
    }
    return RTP_OK;
}

rtp_error_t uvgrtp::rtcp::handle_sdes_packet(uint8_t* packet, size_t size,
    uvgrtp::frame::rtcp_header& header)
{
    if (!packet || !size)
    {
        return RTP_INVALID_VALUE;
    }

    auto frame = new uvgrtp::frame::rtcp_sdes_packet;
    frame->header = header;
    frame->ssrc = ntohl(*(uint32_t*)&packet[4]);

    auto ret = RTP_OK;
    if (srtcp_ && (ret = srtcp_->handle_rtcp_decryption(flags_, frame->ssrc, packet, size)) != RTP_OK)
    {
        delete frame;
        return ret;
    }

    uint32_t rtcp_packet_size = (frame->header.length + 1) * sizeof(uint32_t);

    for (int ptr = 8; ptr < rtcp_packet_size; )
    {
        uvgrtp::frame::rtcp_sdes_item item;

        item.type = packet[ptr++];
        item.length = packet[ptr++];
        item.data = (void*)new uint8_t[item.length];

        memcpy(item.data, &packet[ptr], item.length);
        ptr += item.length; // TODO: Clang warning here
    }

    sdes_mutex_.lock();
    if (sdes_hook_) {
        sdes_hook_(frame);
    } else if (sdes_hook_f_) {
        sdes_hook_f_(std::shared_ptr<uvgrtp::frame::rtcp_sdes_packet>(frame));
    } else if (sdes_hook_u_) {
        sdes_hook_u_(std::unique_ptr<uvgrtp::frame::rtcp_sdes_packet>(frame));
    } else {
        /* Deallocate previous frame from the buffer if it exists, it's going to get overwritten */
        if (participants_[frame->ssrc]->sdes_frame)
        {
            for (auto& item : participants_[frame->ssrc]->sdes_frame->items)
            {
                delete[](uint8_t*)item.data;
            }
            delete participants_[frame->ssrc]->sdes_frame;
        }

        participants_[frame->ssrc]->sdes_frame = frame;
    }
    sdes_mutex_.unlock();

    return RTP_OK;
}

rtp_error_t uvgrtp::rtcp::handle_bye_packet(uint8_t* packet, size_t size)
{
    if (!packet || !size)
    {
        return RTP_INVALID_VALUE;
    }

    for (size_t i = 4; i < size; i += sizeof(uint32_t))
    {
        uint32_t ssrc = ntohl(*(uint32_t*)&packet[i]);

        if (!is_participant(ssrc))
        {
            LOG_WARN("Participants 0x%x is not part of this group!", ssrc);
            continue;
        }

        participants_[ssrc]->socket = nullptr;
        delete participants_[ssrc];
        participants_.erase(ssrc);
    }

    return RTP_OK;
}

rtp_error_t uvgrtp::rtcp::handle_app_packet(uint8_t* packet, size_t size,
    uvgrtp::frame::rtcp_header& header)
{
    if (!packet || !size)
    {
        return RTP_INVALID_VALUE;
    }

    auto frame = new uvgrtp::frame::rtcp_app_packet;
    frame->header = header;
    frame->ssrc = ntohl(*(uint32_t*)&packet[4]);

    auto ret = RTP_OK;
    if (srtcp_ && (ret = srtcp_->handle_rtcp_decryption(flags_, frame->ssrc, packet, size)) != RTP_OK)
    {
        delete frame;
        return ret;
    }

    /* Deallocate previous frame from the buffer if it exists, it's going to get overwritten */
    if (!is_participant(frame->ssrc))
    {
        LOG_WARN("Got an APP packet from an unknown participant");
        add_participant(frame->ssrc);
    }

    uint32_t rtcp_packet_size = (frame->header.length + 1) * sizeof(uint32_t);
    uint32_t application_data_size = rtcp_packet_size
        - (RTCP_HEADER_SIZE + SSRC_CSRC_SIZE + APP_NAME_SIZE);

    // application data is saved to payload
    frame->payload = new uint8_t[application_data_size];

    // copy app name and application-dependent data from network packet to RTCP structures
    memcpy(frame->name, &packet[RTCP_HEADER_SIZE + SSRC_CSRC_SIZE], APP_NAME_SIZE);
    memcpy(frame->payload, &packet[RTCP_HEADER_SIZE + SSRC_CSRC_SIZE + APP_NAME_SIZE], 
           application_data_size);

    app_mutex_.lock();
    if (app_hook_) {
        app_hook_(frame);
    } else if (app_hook_f_) {
        app_hook_f_(std::shared_ptr<uvgrtp::frame::rtcp_app_packet>(frame));
    } else if (app_hook_u_) {
        app_hook_u_(std::unique_ptr<uvgrtp::frame::rtcp_app_packet>(frame));
    } else {

        if (participants_[frame->ssrc]->app_frame)
        {
            delete[] participants_[frame->ssrc]->app_frame->payload;
            delete   participants_[frame->ssrc]->app_frame;
        }

        participants_[frame->ssrc]->app_frame = frame;
    }
    app_mutex_.unlock();

    return RTP_OK;
}

rtp_error_t uvgrtp::rtcp::handle_receiver_report_packet(uint8_t* packet, size_t size,
    uvgrtp::frame::rtcp_header& header)
{
    if (!packet || !size)
    {
        return RTP_INVALID_VALUE;
    }

    auto frame = new uvgrtp::frame::rtcp_receiver_report;
    frame->header = header;
    frame->ssrc = ntohl(*(uint32_t*)&packet[RTCP_HEADER_SIZE]);

    auto ret = RTP_OK;
    if (srtcp_ && (ret = srtcp_->handle_rtcp_decryption(flags_, frame->ssrc, packet, size)) != RTP_OK)
    {
        delete frame;
        return ret;
    }

    /* Receiver Reports are sent from participant that don't send RTP packets
     * This means that the sender of this report is not in the participants_ map
     * but rather in the initial_participants_ vector
     *
     * Check if that's the case and if so, move the entry from initial_participants_ to participants_ */
    if (!is_participant(frame->ssrc))
    {
        LOG_WARN("Got a Receiver Report from an unknown participant");
        add_participant(frame->ssrc);
    }

    if (!frame->header.count)
    {
        LOG_ERROR("Receiver Report cannot have 0 report blocks!");
        return RTP_INVALID_VALUE;
    }

    read_reports(packet, size, frame->header.count, false, frame->report_blocks);

    rr_mutex_.lock();
    if (receiver_hook_) {
        receiver_hook_(frame);
    } else if (rr_hook_f_) {
        rr_hook_f_(std::shared_ptr<uvgrtp::frame::rtcp_receiver_report>(frame));
    } else if (rr_hook_u_) {
        rr_hook_u_(std::unique_ptr<uvgrtp::frame::rtcp_receiver_report>(frame));
    } else {
        /* Deallocate previous frame from the buffer if it exists, it's going to get overwritten */
        if (participants_[frame->ssrc]->rr_frame)
        {
            delete participants_[frame->ssrc]->rr_frame;
        }

        participants_[frame->ssrc]->rr_frame = frame;
    }
    rr_mutex_.unlock();

    return RTP_OK;
}

rtp_error_t uvgrtp::rtcp::handle_sender_report_packet(uint8_t* packet, size_t size,
    uvgrtp::frame::rtcp_header& header)
{
    if (!packet || !size)
    {
        return RTP_INVALID_VALUE;
    }

    auto frame = new uvgrtp::frame::rtcp_sender_report;
    frame->header = header;
    frame->ssrc = ntohl(*(uint32_t*)&packet[4]);

    auto ret = RTP_OK;
    if (srtcp_ && (ret = srtcp_->handle_rtcp_decryption(flags_, frame->ssrc, packet, size)) != RTP_OK)
    {
        delete frame;
        return ret;
    }

    if (!is_participant(frame->ssrc))
    {
        LOG_WARN("Sender Report received from an unknown participant");
        add_participant(frame->ssrc);
    }

    frame->sender_info.ntp_msw =    ntohl(*(uint32_t*)&packet[8]);
    frame->sender_info.ntp_lsw =    ntohl(*(uint32_t*)&packet[12]);
    frame->sender_info.rtp_ts =     ntohl(*(uint32_t*)&packet[16]);
    frame->sender_info.pkt_cnt =    ntohl(*(uint32_t*)&packet[20]);
    frame->sender_info.byte_cnt =   ntohl(*(uint32_t*)&packet[24]);

    participants_[frame->ssrc]->stats.sr_ts = uvgrtp::clock::hrc::now();
    participants_[frame->ssrc]->stats.lsr =
        ((frame->sender_info.ntp_msw & 0xffff) << 16) |
        (frame->sender_info.ntp_lsw >> 16);

    read_reports(packet, size, frame->header.count, true, frame->report_blocks);

    sr_mutex_.lock();
    if (sender_hook_) {
        sender_hook_(frame);
    } else if (sr_hook_f_) {
        sr_hook_f_(std::shared_ptr<uvgrtp::frame::rtcp_sender_report>(frame));
    } else if (sr_hook_u_) {
        sr_hook_u_(std::unique_ptr<uvgrtp::frame::rtcp_sender_report>(frame));
    } else {
        /* Deallocate previous frame from the buffer if it exists, it's going to get overwritten */
        if (participants_[frame->ssrc]->sr_frame)
        {
            delete participants_[frame->ssrc]->sr_frame;
        }

        participants_[frame->ssrc]->sr_frame = frame;
    }
    sr_mutex_.unlock();

    return RTP_OK;
}

void uvgrtp::rtcp::read_rtcp_header(const uint8_t* packet, uvgrtp::frame::rtcp_header& header)
{
    header.version = (packet[0] >> 6) & 0x3;
    header.padding = (packet[0] >> 5) & 0x1;

    header.pkt_type = packet[1] & 0xff;

    if (header.pkt_type == uvgrtp::frame::RTCP_FT_APP)
    {
        header.pkt_subtype = packet[0] & 0x1f;
    } else {
        header.count = packet[0] & 0x1f;
    }

    header.length = ntohs(*(uint16_t*)&packet[2]);
}

void uvgrtp::rtcp::read_reports(const uint8_t* packet, size_t size, uint8_t count, bool has_sender_block,
                                std::vector<uvgrtp::frame::rtcp_report_block>& reports)
{
    uint32_t report_section = RTCP_HEADER_SIZE + SSRC_CSRC_SIZE;

    if (has_sender_block)
    {
        report_section += SENDER_INFO_SIZE;
    }

    for (int i = 0; i < count; ++i)
    {
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
        } else {
            LOG_DEBUG("Received rtcp packet is smaller than the indicated number of reports!");
        }
    }
}

rtp_error_t uvgrtp::rtcp::send_rtcp_packet_to_participants(uint8_t* frame, size_t frame_size, bool encrypt)
{
    rtp_error_t ret = RTP_OK;

    if (encrypt && srtcp_ && 
        (ret = srtcp_->handle_rtcp_encryption(flags_, rtcp_pkt_sent_count_, ssrc_, frame, frame_size)) != RTP_OK)
    {
        LOG_DEBUG("Encryption failed. Not sending packet");
        delete[] frame;
        return ret;
    }

    for (auto& p : participants_)
    {
        if (p.second->socket != nullptr)
        {
            if ((ret = p.second->socket->sendto(p.second->address, frame, frame_size, 0)) != RTP_OK)
            {
                LOG_ERROR("Sending rtcp packet with sendto() failed!");
                break;
            }

            update_rtcp_bandwidth(frame_size);
        }
        else
        {
            LOG_ERROR("Tried to send RTCP packet when socket does not exist!");
        }
    }
    delete[] frame;

    return ret;
}

rtp_error_t uvgrtp::rtcp::generate_report()
{
    rtcp_pkt_sent_count_++;

    uint16_t reports = 0;
    for (auto& p : participants_)
    {
        if (p.second->stats.received_rtp_packet)
        {
            ++reports;
        }
    }

    size_t sdes_packet_size = get_sdes_packet_size(ourItems_);

    size_t compound_packet_size = get_rr_packet_size(flags_, reports) + sdes_packet_size;

    if (our_role_ == SENDER && our_stats.sent_rtp_packet)
    {
        compound_packet_size = get_sr_packet_size(flags_, reports) + sdes_packet_size;
    }
    uint8_t* frame = new uint8_t[compound_packet_size];
    memset(frame, 0, compound_packet_size);

    // see https://datatracker.ietf.org/doc/html/rfc3550#section-6.4.1

    int ptr = 0;
    if (our_role_ == SENDER && our_stats.sent_rtp_packet)
    {
        // sender reports have sender information in addition compared to receiver reports
        size_t sender_report_size = get_sr_packet_size(flags_, reports);

        LOG_DEBUG("Generating RTCP Sender report size: %li", sender_report_size);

        /* TODO: The clock would be better to start with first sent RTP packet.
         * In reality it should be provided by user which I think is implemented? */
        if (clock_start_ == 0)
        {
          clock_start_ = uvgrtp::clock::ntp::now();
        }

        /* TODO: The RTP timestamp should be from an actual RTP packet and NTP timestamp should be the one
           corresponding to it. */
        uint64_t ntp_ts = uvgrtp::clock::ntp::now();
        uint64_t rtp_ts = rtp_ts_start_ + (uvgrtp::clock::ntp::diff(clock_start_, ntp_ts))
            * float(clock_rate_ / 1000);

        construct_rtcp_header(frame, ptr, sender_report_size, reports, uvgrtp::frame::RTCP_FT_SR);
        construct_ssrc(frame, ptr, ssrc_);
        construct_sender_info(frame, ptr, ntp_ts, rtp_ts, our_stats.sent_pkts, our_stats.sent_bytes);

        our_stats.sent_rtp_packet = false;

    } else { // RECEIVER
        size_t receiver_report_size = get_rr_packet_size(flags_, reports);
        LOG_DEBUG("Generating RTCP Receiver report size: %li", receiver_report_size);
        construct_rtcp_header(frame, ptr, receiver_report_size, reports, uvgrtp::frame::RTCP_FT_RR);
        construct_ssrc(frame, ptr, ssrc_);
    }

    // the report blocks for sender or receiver report. Both have same reports.
    for (auto& p : participants_)
    {
        // only add report blocks if we have received data from them
        if (p.second->stats.received_rtp_packet)
        {
            uint32_t dropped_packets = p.second->stats.dropped_pkts;
            // TODO: This should be the number of packets lost compared to number of packets expected (see fraction lost in RFC 3550)
            // see https://datatracker.ietf.org/doc/html/rfc3550#appendix-A.3
            uint8_t fraction = dropped_packets ? p.second->stats.received_bytes / dropped_packets : 0;

            uint64_t diff = (u_long)uvgrtp::clock::hrc::diff_now(p.second->stats.sr_ts);
            uint32_t dlrs = uvgrtp::clock::ms_to_jiffies(diff);

            /* calculate delay of last SR only if SR has been received at least once */
            if (p.second->stats.lsr == 0)
            {
                dlrs = 0;
            }

            construct_report_block(frame, ptr, p.first, fraction, dropped_packets,
                p.second->stats.cycles, p.second->stats.max_seq, p.second->stats.jitter, 
                p.second->stats.lsr, dlrs);

            // we only send reports if there is something to report since last report
            p.second->stats.received_rtp_packet = false;
        }
    }

    // header construction also adds our ssrc
    if (!construct_rtcp_header(frame, ptr, sdes_packet_size, num_receivers_, 
                               uvgrtp::frame::RTCP_FT_SDES) ||
        !construct_ssrc(frame, ptr, ssrc_) ||
        !construct_sdes_items(frame, ptr, ourItems_))
    {
        delete[] frame;
        return RTP_GENERIC_ERROR;
    }

    LOG_DEBUG("Sending RTCP report compound packet, Total size: %li, SDES packet size: %li", 
        compound_packet_size, sdes_packet_size);

    return send_rtcp_packet_to_participants(frame, compound_packet_size, true);
}

rtp_error_t uvgrtp::rtcp::send_sdes_packet(const std::vector<uvgrtp::frame::rtcp_sdes_item>& items)
{
    if (items.empty())
    {
        LOG_ERROR("Cannot send an empty SDES packet!");
        return RTP_INVALID_VALUE;
    }

    size_t rtcp_packet_size = get_sdes_packet_size(items);
    uint8_t* frame = new uint8_t[rtcp_packet_size];
    memset(frame, 0, rtcp_packet_size);

    int ptr = 0;

    if (!construct_rtcp_header(frame, ptr, rtcp_packet_size, 1, 
                               uvgrtp::frame::RTCP_FT_SDES) ||
        !construct_sdes_chunk(frame, ptr, ssrc_, items))
    {
        delete[] frame;
        return RTP_GENERIC_ERROR;
    }

    // TODO: disable encryption because sdes doesn't actually have ssrc?
    return send_rtcp_packet_to_participants(frame, rtcp_packet_size, true);
}

rtp_error_t uvgrtp::rtcp::send_bye_packet(std::vector<uint32_t> ssrcs)
{
    // ssrcs contains all our ssrcs which we usually have one unless we are a mixer
    if (ssrcs.empty())
    {
        LOG_WARN("Source Count in RTCP BYE packet is 0");
    }

    size_t rtcp_packet_size = get_bye_packet_size(ssrcs);
    uint8_t* frame = new uint8_t[rtcp_packet_size];
    memset(frame, 0, rtcp_packet_size);

    rtp_error_t ret = RTP_OK;
    int ptr = 0;
    uint16_t secondField = (ssrcs.size() & 0x1f);
    // header construction does not add our ssrc for BYE
    if (!construct_rtcp_header(frame, ptr, rtcp_packet_size, secondField, 
                               uvgrtp::frame::RTCP_FT_BYE) ||
        !construct_ssrc(frame, ptr, ssrc_) ||
        !construct_bye_packet(frame, ptr, ssrcs))
    {
        delete[] frame;
        return RTP_GENERIC_ERROR;
    }

    // TODO: Enable encryption because bye actually has ssrc
    return send_rtcp_packet_to_participants(frame, rtcp_packet_size, false);
}

rtp_error_t uvgrtp::rtcp::send_app_packet(const char* name, uint8_t subtype,
    size_t payload_len, const uint8_t* payload)
{
    size_t rtcp_packet_size = get_app_packet_size(payload_len);
    uint8_t* frame = new uint8_t[rtcp_packet_size];
    memset(frame, 0, rtcp_packet_size);

    int ptr = 0;
    uint16_t secondField = (subtype & 0x1f);

    if (!construct_rtcp_header(frame, ptr, rtcp_packet_size, secondField, 
                               uvgrtp::frame::RTCP_FT_APP) ||
        !construct_ssrc(frame, ptr, ssrc_) ||
        !construct_app_packet(frame, ptr, name, payload, payload_len))
    {
        delete[] frame;
        return RTP_GENERIC_ERROR;
    }

    return send_rtcp_packet_to_participants(frame, rtcp_packet_size, true);
}

void uvgrtp::rtcp::set_session_bandwidth(int kbps)
{
    interval_ms_ = 1000*360 / kbps; // the reduced minimum (see section 6.2 in RFC 3550)

    if (interval_ms_ > DEFAULT_RTCP_INTERVAL_MS)
    {
        interval_ms_ = DEFAULT_RTCP_INTERVAL_MS;
    }
    // TODO: This should follow the algorithm specified in RFC 3550 appendix-A.7
}