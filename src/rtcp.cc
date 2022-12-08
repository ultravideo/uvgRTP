#include "uvgrtp/rtcp.hh"

#include "uvgrtp/util.hh"
#include "uvgrtp/frame.hh"

#include "socket.hh"
#include "hostname.hh"
#include "poll.hh"
#include "rtp.hh"
#include "debug.hh"
#include "srtp/srtcp.hh"
#include "rtcp_packets.hh"

#include "global.hh"


#ifndef _WIN32
#include <sys/time.h>
#endif

#include <cassert>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <set>


/* TODO: explain these constants */
const uint32_t RTP_SEQ_MOD    = 1 << 16;
const uint32_t MIN_SEQUENTIAL = 2;
const uint32_t MAX_DROPOUT    = 3000;
const uint32_t MAX_MISORDER   = 100;
const uint32_t DEFAULT_RTCP_INTERVAL_MS = 5000;
const int MAX_PACKET = 65536;

constexpr int ESTIMATED_MAX_RECEPTION_TIME_MS = 10;

const uint32_t MAX_SUPPORTED_PARTICIPANTS = 31;

uvgrtp::rtcp::rtcp(std::shared_ptr<uvgrtp::rtp> rtp, std::shared_ptr<std::atomic_uint> ssrc, std::string cname, int rce_flags):
    rce_flags_(rce_flags), our_role_(RECEIVER), 
    tp_(0), tc_(0), tn_(0), pmembers_(0),
    members_(0), senders_(0), rtcp_bandwidth_(0),
    we_sent_(false), avg_rtcp_pkt_pize_(0), rtcp_pkt_count_(0),
    rtcp_pkt_sent_count_(0), initial_(true), ssrc_(ssrc),
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
    interval_ms_(DEFAULT_RTCP_INTERVAL_MS),
    ourItems_(),
    bye_ssrcs_(false),
    hooked_app_(false),
    mtu_size_(MAX_IPV4_PAYLOAD)
{
    clock_rate_   = rtp->get_clock_rate();

    clock_start_  = 0;
    rtp_ts_start_ = 0;

    report_generator_   = nullptr;

    srtcp_        = nullptr;

    zero_stats(&our_stats);

    if (cname.length() > 255)
    {
        UVG_LOG_ERROR("Our CName is too long");
    }
    else
    {
        // items should not have null termination
        const char* c = cname.c_str();
        memcpy(cname_, c, cname.length());
        uint8_t length = (uint8_t)cname.length();

        cnameItem_.type = 1;
        cnameItem_.length = length;
        cnameItem_.data = (uint8_t*)cname_;

        ourItems_.push_back(cnameItem_);
    }
}

uvgrtp::rtcp::rtcp(std::shared_ptr<uvgrtp::rtp> rtp, std::shared_ptr<std::atomic_uint> ssrc, std::string cname,
    std::shared_ptr<uvgrtp::srtcp> srtcp, int rce_flags):
    rtcp(rtp, ssrc, cname, rce_flags)
{
    srtcp_ = srtcp;
}

uvgrtp::rtcp::~rtcp()
{
    if (active_)
    {
        stop();
    }

    cleanup_participants();

    ourItems_.clear();
    sockets_.clear();
}

void uvgrtp::rtcp::cleanup_participants()
{
    UVG_LOG_DEBUG("Removing all participants");

    participants_mutex_.lock();
    /* free all receiver statistic structs */
    for (auto& participant : participants_)
    {
        free_participant(std::move(participant.second));
    }
    participants_.clear();
    participants_mutex_.unlock();

    for (auto& participant : initial_participants_)
    {
        free_participant(std::move(participant));
    }
    initial_participants_.clear();
}

uvgrtp::rtcp_app_packet::rtcp_app_packet(const char* name, uint8_t subtype, uint32_t payload_len, const uint8_t* payload)
{
    uint8_t* packet_payload = new uint8_t[payload_len];
    memcpy(packet_payload, payload, payload_len);

    char* packet_name = new char[APP_NAME_SIZE];
    memcpy(packet_name, name, APP_NAME_SIZE);

    this->name = packet_name;
    this->payload = packet_payload;
    this->subtype = subtype;
    this->payload_len = payload_len;
}

uvgrtp::rtcp_app_packet::~rtcp_app_packet() {
    delete[] name;
    delete[] payload;
}

void uvgrtp::rtcp::free_participant(std::unique_ptr<rtcp_participant> participant)
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
        for (auto& chunk : participant->sdes_frame->chunks)
        {
            for (auto& item : chunk.items)
            {
                if (item.data != nullptr)
                {
                    delete[] item.data;
                }
            }
        }

        delete participant->sdes_frame;
    }
    if (participant->app_frame)
    {
        if (participant->app_frame->payload != nullptr)
        {
            delete[] participant->app_frame->payload;
        }
        delete participant->app_frame;
    }
}

rtp_error_t uvgrtp::rtcp::start()
{
    if (sockets_.empty())
    {
        UVG_LOG_ERROR("Cannot start RTCP Runner because no connections have been initialized");
        return RTP_INVALID_VALUE;
    }
    active_ = true;

    report_generator_.reset(new std::thread(rtcp_runner, this, interval_ms_));

    return RTP_OK;
}

rtp_error_t uvgrtp::rtcp::stop()
{
    UVG_LOG_DEBUG("Stopping RTCP");

    // TODO: Make thread safe. I think this kind of works, but not in a flexible way
    if (!active_)
    {
        cleanup_participants();
        return RTP_OK;
    }

    active_ = false;

    if (report_generator_ && report_generator_->joinable())
    {
        UVG_LOG_DEBUG("Waiting for RTCP loop to exit");
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
    return uvgrtp::rtcp::send_bye_packet({ *ssrc_.get() });
}

void uvgrtp::rtcp::rtcp_runner(rtcp* rtcp, int interval)
{
    UVG_LOG_INFO("RTCP instance created! RTCP interval: %i ms", interval);

    // RFC 3550 says to wait half interval before sending first report
    int initial_sleep_ms = interval / 2;
    UVG_LOG_DEBUG("Sleeping for %i ms before sending first RTCP report", initial_sleep_ms);
    std::this_thread::sleep_for(std::chrono::milliseconds(initial_sleep_ms));

    std::unique_ptr<uint8_t[]> buffer = std::unique_ptr<uint8_t[]>(new uint8_t[MAX_PACKET]);

    uvgrtp::clock::hrc::hrc_t start = uvgrtp::clock::hrc::now();

    int i = 0;
    while (rtcp->is_active())
    {
        long int next_sendslot = i * interval;
        uint32_t run_time = (uint32_t)uvgrtp::clock::hrc::diff_now(start);
        long int diff_ms = next_sendslot - run_time;

        rtp_error_t ret = RTP_OK;
        if (diff_ms <= 0)
        {
            ++i;

            UVG_LOG_DEBUG("Sending RTCP report number %i at time slot %i ms", i, next_sendslot);

            if ((ret = rtcp->generate_report()) != RTP_OK && ret != RTP_NOT_READY)
            {
                UVG_LOG_ERROR("Failed to send RTCP status report!");
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

            ret = uvgrtp::poll::poll(rtcp->get_sockets(), buffer.get(), MAX_PACKET, poll_timout, &nread);

            if (ret == RTP_OK && nread > 0)
            {
                (void)rtcp->handle_incoming_packet(buffer.get(), (size_t)nread);
            } else if (ret == RTP_INTERRUPTED) {
                /* do nothing */
            } else {
                UVG_LOG_ERROR("poll failed, %d", ret);
                break; // TODO the sockets should be manages so that this is not needed
            }
        } else { // sleep until it is time to send the report
            std::this_thread::sleep_for(std::chrono::milliseconds(diff_ms));
        }
    }

    UVG_LOG_DEBUG("Exited RTCP loop");
}

rtp_error_t uvgrtp::rtcp::set_sdes_items(const std::vector<uvgrtp::frame::rtcp_sdes_item>& items)
{
    bool hasCname = false;

    std::set<unsigned int> to_ignore;

    // find invalid items and check if cname is already included
    for (unsigned int i = 0; i < items.size(); ++i)
    {
        if (items.at(i).type == 0)
        {
            UVG_LOG_WARN("Invalid item type 0 found at index %lu, removing item", i);
            to_ignore.insert(i);
        }
        else if (items.at(i).type == 1)
        {
            hasCname = true;
            UVG_LOG_DEBUG("Found CName in sdes items, not adding pregenerated");
            break;
        }
    }

    ourItems_.clear();
    if (!hasCname)
    {
        ourItems_.push_back(cnameItem_);
    }

    // add all items expect ones set for us to ignore
    for (unsigned int i = 0; i < items.size(); ++i)
    {
        if (to_ignore.find(i) == to_ignore.end())
        {
            ourItems_.push_back(items.at(i));
        }
    }

    return RTP_OK;
}

rtp_error_t uvgrtp::rtcp::add_participant(std::string src_addr, std::string dst_addr, uint16_t dst_port, uint16_t src_port, uint32_t clock_rate)
{
    if (dst_addr == "" || !dst_port || !src_port)
    {
        UVG_LOG_ERROR("Invalid values given (%s, %d, %d), cannot create RTCP instance",
                dst_addr.c_str(), dst_port, src_port);
        return RTP_INVALID_VALUE;
    }

    rtp_error_t ret;
    std::unique_ptr<rtcp_participant> p = std::unique_ptr<rtcp_participant>(new rtcp_participant());

    zero_stats(&p->stats);

    p->socket = std::shared_ptr<uvgrtp::socket> (new uvgrtp::socket(0));

    if ((ret = p->socket->init(AF_INET, SOCK_DGRAM, 0)) != RTP_OK)
    {
        free_participant(std::move(p));
        return ret;
    }

    int enable = 1;

    if ((ret = p->socket->setsockopt(SOL_SOCKET, SO_REUSEADDR, (const char *)&enable, sizeof(int))) != RTP_OK)
    {
        free_participant(std::move(p));
        return ret;
    }

#ifdef _WIN32
    /* Make the socket non-blocking */
    int enabled = 1;

    if (::ioctlsocket(p->socket->get_raw_socket(), FIONBIO, (u_long *)&enabled) < 0)
    {
        UVG_LOG_ERROR("Failed to make the socket non-blocking!");
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
        free_participant(std::move(p));
        return ret;
    }

    

    if (src_addr != "")
    {
        UVG_LOG_INFO("Binding RTCP to port %s:%d", src_addr.c_str(), src_port);
        sockaddr_in bind_addr = p->socket->create_sockaddr(AF_INET, src_addr, src_port);
        if ((ret = p->socket->bind(bind_addr)) != RTP_OK)
        {
            free_participant(std::move(p));
            return ret;
        }
    }
    else
    {
        UVG_LOG_INFO("Binding RTCP to port %d (source port)", src_port);
        if ((ret = p->socket->bind(AF_INET, INADDR_ANY, src_port)) != RTP_OK)
        {
            free_participant(std::move(p));
            return ret;
        }
    }

    p->role             = RECEIVER;
    p->address          = p->socket->create_sockaddr(AF_INET, dst_addr, dst_port);
    p->stats.clock_rate = clock_rate;

    sockets_.push_back(p->socket);
    initial_participants_.push_back(std::move(p));

    return RTP_OK;
}

rtp_error_t uvgrtp::rtcp::add_participant(uint32_t ssrc)
{
    if (num_receivers_ == MAX_SUPPORTED_PARTICIPANTS)
    {
        UVG_LOG_ERROR("Maximum number of RTCP participants reached.");
        // TODO: Support more participants by sending multiple messages at the same time
        return RTP_GENERIC_ERROR;
    }

    participants_mutex_.lock();
    /* RTCP is not in use for this media stream,
     * create a "fake" participant that is only used for storing statistics information */
    if (initial_participants_.empty())
    {
        participants_[ssrc] = std::unique_ptr<rtcp_participant> (new rtcp_participant());
        zero_stats(&participants_[ssrc]->stats);
    } else {
        participants_[ssrc] = std::move(initial_participants_.back());
        initial_participants_.pop_back();
    }
    num_receivers_++;

    participants_[ssrc]->rr_frame    = nullptr;
    participants_[ssrc]->sr_frame    = nullptr;
    participants_[ssrc]->sdes_frame  = nullptr;
    participants_[ssrc]->app_frame   = nullptr;
    participants_mutex_.unlock();

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

    send_app_mutex_.lock();
    outgoing_app_hooks_.clear();
    send_app_mutex_.unlock();
    hooked_app_ = false;

    return RTP_OK;
}

rtp_error_t uvgrtp::rtcp::remove_send_app_hook(std::string app_name)
{
    std::lock_guard<std::mutex> guard(send_app_mutex_);
    if (outgoing_app_hooks_.find(app_name) == outgoing_app_hooks_.end()) {
        return RTP_INVALID_VALUE;
    }
    outgoing_app_hooks_.erase(app_name);
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

rtp_error_t uvgrtp::rtcp::install_send_app_hook(std::string app_name, std::function<void(uint32_t& payload_len, uint8_t& subtype, std::vector<uint8_t>& payload)> app_sending_func)
{
    if (!app_sending_func || app_name.empty() || app_name.size() > 4) {
        return RTP_INVALID_VALUE;
    }
    if (outgoing_app_hooks_.find(app_name) != outgoing_app_hooks_.end()) {
        UVG_LOG_WARN("Replacing existing outgoing APP hook");
    }

    hooked_app_ = true;
    {
        std::lock_guard<std::mutex> lock(send_app_mutex_);
        outgoing_app_hooks_[app_name] = app_sending_func;
    }
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
    std::lock_guard prtcp_lock(participants_mutex_);
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
    std::lock_guard prtcp_lock(participants_mutex_);
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
    std::lock_guard prtcp_lock(participants_mutex_);
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
    std::lock_guard prtcp_lock(participants_mutex_);
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

std::vector<std::shared_ptr<uvgrtp::socket>>& uvgrtp::rtcp::get_sockets()
{
    return sockets_;
}

std::vector<uint32_t> uvgrtp::rtcp::get_participants() const
{
    std::vector<uint32_t> ssrcs;

    for (auto& i : participants_)
    {
        std::lock_guard prtcp_lock(participants_mutex_);
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
    std::lock_guard prtcp_lock(participants_mutex_);
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
        UVG_LOG_ERROR("Payload size larger than uint32 max which is not supported by RFC 3550");
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
    participants_mutex_.lock();
    /* Set the probation to MIN_SEQUENTIAL (2)
     *
     * What this means is that we must receive at least two packets from SSRC
     * with sequential RTP sequence numbers for this peer to be considered valid */
    participants_[frame->header.ssrc]->probation = MIN_SEQUENTIAL;

    /* This is the first RTP frame from remote to frame->header.timestamp represents t = 0
     * Save the timestamp and current NTP timestamp so we can do jitter calculations later on */
    participants_[frame->header.ssrc]->stats.initial_rtp = frame->header.timestamp;
    participants_[frame->header.ssrc]->stats.initial_ntp = uvgrtp::clock::ntp::now();
    participants_mutex_.unlock();

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
        UVG_LOG_ERROR("Sent bytes overflow");
    }

    our_stats.sent_pkts  += 1;
    our_stats.sent_bytes += (uint32_t)pkt_size;
    our_stats.sent_rtp_packet = true;

    return RTP_OK;
}

rtp_error_t uvgrtp::rtcp::init_participant_seq(uint32_t ssrc, uint16_t base_seq)
{
    std::lock_guard prtcp_lock(participants_mutex_);
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
    std::unique_lock prtcp_lock(participants_mutex_);
    if (participants_.find(ssrc) == participants_.end())
    {
        UVG_LOG_ERROR("Did not find participant SSRC when updating seq");
        return RTP_GENERIC_ERROR;
    }

    uint16_t udelta = seq - participants_[ssrc]->stats.max_seq;

    /* Source is not valid until MIN_SEQUENTIAL packets with
    * sequential sequence numbers have been received.  */
    if (participants_[ssrc]->probation)
    {
       /* packet is in sequence */
       if (seq == participants_[ssrc]->stats.max_seq + 1)
       {
           participants_[ssrc]->probation--;
           participants_[ssrc]->stats.max_seq = seq;
           if (!participants_[ssrc]->probation)
           {
               prtcp_lock.unlock();
               uvgrtp::rtcp::init_participant_seq(ssrc, seq);
               return RTP_OK;
           }
       } else {
           participants_[ssrc]->probation = MIN_SEQUENTIAL - 1;
           participants_[ssrc]->stats.max_seq = seq;
       }

       return RTP_NOT_READY;
    } else if (udelta < MAX_DROPOUT) {
       /* in order, with permissible gap */
       if (seq < participants_[ssrc]->stats.max_seq)
       {
           /* Sequence number wrapped - count another 64K cycle.  */
           participants_[ssrc]->stats.cycles += 1;
       }
       participants_[ssrc]->stats.max_seq = seq;
    } else if (udelta <= RTP_SEQ_MOD - MAX_MISORDER) {
       /* the sequence number made a very large jump */
       if (seq == participants_[ssrc]->stats.bad_seq)
       {
           /* Two sequential packets -- assume that the other side
            * restarted without telling us so just re-sync
            * (i.e., pretend this was the first packet).  */
           prtcp_lock.unlock();
           uvgrtp::rtcp::init_participant_seq(ssrc, seq);
           prtcp_lock.lock();
       } else {
           participants_[ssrc]->stats.bad_seq = (seq + 1) & (RTP_SEQ_MOD - 1);
           UVG_LOG_ERROR("Invalid sequence number. Seq jump: %u -> %u", participants_[ssrc]->stats.max_seq, seq);
           return RTP_GENERIC_ERROR;
       }
    } else {
       /* duplicate or reordered packet */
    }

    return RTP_OK;
}

rtp_error_t uvgrtp::rtcp::reset_rtcp_state(uint32_t ssrc)
{
    std::lock_guard prtcp_lock(participants_mutex_);
    if (participants_.find(ssrc) != participants_.end())
    {
        return RTP_SSRC_COLLISION;
    }

    zero_stats(&our_stats);

    return RTP_OK;
}

bool uvgrtp::rtcp::collision_detected(uint32_t ssrc, const sockaddr_in& src_addr) const
{
    std::lock_guard prtcp_lock(participants_mutex_);
    if (participants_.find(ssrc) == participants_.end())
    {
        return false;
    }

    if (src_addr.sin_port        != participants_.at(ssrc)->address.sin_port &&
        src_addr.sin_addr.s_addr != participants_.at(ssrc)->address.sin_addr.s_addr)
    {
        return true;
    }

    return false;
}

void uvgrtp::rtcp::update_session_statistics(const uvgrtp::frame::rtp_frame *frame)
{
    std::lock_guard prtcp_lock(participants_mutex_);
    participants_[frame->header.ssrc]->stats.received_rtp_packet = true;

    participants_[frame->header.ssrc]->stats.received_pkts  += 1;
    participants_[frame->header.ssrc]->stats.received_bytes += (uint32_t)frame->payload_len;

    /* calculate number of dropped packets */
    int extended_max = (static_cast<int>(participants_[frame->header.ssrc]->stats.cycles) << 16) + 
        participants_[frame->header.ssrc]->stats.max_seq;
    int expected     = extended_max - participants_[frame->header.ssrc]->stats.base_seq + 1;

    int dropped = expected - participants_[frame->header.ssrc]->stats.received_pkts;
    participants_[frame->header.ssrc]->stats.dropped_pkts = dropped >= 0 ? dropped : 0;

    // the arrival time expressed as an RTP timestamp
    uint32_t arrival = participants_[frame->header.ssrc]->stats.initial_rtp +
        (uint32_t)uvgrtp::clock::ntp::diff_now(participants_[frame->header.ssrc]->stats.initial_ntp)*
        (participants_[frame->header.ssrc]->stats.clock_rate / 1000);

    // calculate interarrival jitter. See RFC 3550 A.8
    uint32_t transit = arrival - frame->header.timestamp; // A.8: int transit = arrival - r->ts
    uint32_t trans_difference = std::abs((int)(transit - participants_[frame->header.ssrc]->stats.transit));

    // update statistics
    participants_[frame->header.ssrc]->stats.transit = transit;
    participants_[frame->header.ssrc]->stats.jitter += (1.f / 16.f) * 
        ((double)trans_difference - participants_[frame->header.ssrc]->stats.jitter);
}

/* RTCP packet handler is responsible for doing two things:
 *
 * - it checks whether the packet is coming from an existing user and if so,
 *   updates that user's session statistics. If the packet is coming from a user,
 *   the user is put on probation where they will stay until enough valid packets
 *   have been received.
 * - it keeps track of participants' SSRCs and if a collision
 *   is detected, the RTP context is updated */
rtp_error_t uvgrtp::rtcp::recv_packet_handler(void *arg, int rce_flags, frame::rtp_frame **out)
{
    (void)rce_flags;

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
            UVG_LOG_ERROR("Failed to initiate new participant");
            return RTP_GENERIC_ERROR;
        }
    } else if ((ret = rtcp->update_participant_seq(frame->header.ssrc, frame->header.seq)) != RTP_OK) {
        if (ret == RTP_NOT_READY) {
            return RTP_OK;
        }
        else {
            UVG_LOG_ERROR("Failed to update participant with seq %u", frame->header.seq);
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
    ssize_t pkt_size = -RTP_HDR_SIZE;

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

size_t uvgrtp::rtcp::rtcp_length_in_bytes(uint16_t length)
{
    size_t expanded_length = length;

    // the length field is the rtcp packet size measured in 32-bit words - 1
    return (expanded_length + 1)* sizeof(uint32_t);
}

rtp_error_t uvgrtp::rtcp::handle_incoming_packet(uint8_t *buffer, size_t size)
{
    if (!buffer || !size)
    {
        return RTP_INVALID_VALUE;
    }

    UVG_LOG_DEBUG("Received an RTCP packet with size: %li", size);

    size_t read_ptr = 0;
    size_t remaining_size = size;

    int packets = 0;

    update_rtcp_bandwidth(size);

    rtp_error_t ret = RTP_OK;

    uint32_t sender_ssrc = 0;

    // decrypt the whole compound packet
    if (size > RTCP_HEADER_SIZE + SSRC_CSRC_SIZE)
    {
        sender_ssrc = ntohl(*(uint32_t*)& buffer[read_ptr + RTCP_HEADER_SIZE]);
        if (srtcp_ && (ret = srtcp_->handle_rtcp_decryption(rce_flags_, sender_ssrc, 
            buffer + RTCP_HEADER_SIZE + SSRC_CSRC_SIZE, size)) != RTP_OK)
        {
            UVG_LOG_ERROR("Failed at decryption");
            return ret;
        }
    }

    // this handles each separate rtcp packet in a compound packet
    while (remaining_size > 0)
    {
        ++packets;
        if (remaining_size < RTCP_HEADER_SIZE)
        {
            UVG_LOG_ERROR("Didn't get enough data for an rtcp header. Packet # %i Got data: %lli",
                packets, remaining_size);
            return RTP_INVALID_VALUE;
        }

        uvgrtp::frame::rtcp_header header;
        read_rtcp_header(buffer, read_ptr, header);

        size_t size_of_rtcp_packet = rtcp_length_in_bytes(header.length);

        /* Possible packet padding means header.length is only reliable way to find next packet.
         * We have to substract the size of header, since it was added when reading the header. */
        size_t packet_end = read_ptr - RTCP_HEADER_SIZE + size_of_rtcp_packet;

        UVG_LOG_DEBUG("Handling packet # %i with size %li and remaining packet amount %li",
            packets, size_of_rtcp_packet, remaining_size);

        if (header.version != 0x2)
        {
            UVG_LOG_ERROR("Packet # %i has invalid header version %u", packets, header.version);
            return RTP_INVALID_VALUE;
        }

        if (remaining_size < size_of_rtcp_packet)
        {
            UVG_LOG_ERROR("Received a partial RTCP packet, not supported!");
            return RTP_NOT_SUPPORTED;
        }

        // TODO: I think we can?
        if (header.padding)
        {
            UVG_LOG_ERROR("Cannot handle padded packets!");
            return RTP_INVALID_VALUE;
        }

        if (header.pkt_type > uvgrtp::frame::RTCP_FT_APP ||
            header.pkt_type < uvgrtp::frame::RTCP_FT_SR)
        {
            UVG_LOG_ERROR("Invalid packet type (%u)!", header.pkt_type);
            return RTP_INVALID_VALUE;
        }

        ret = RTP_INVALID_VALUE;

        switch (header.pkt_type)
        {
            case uvgrtp::frame::RTCP_FT_SR:
                ret = handle_sender_report_packet(buffer, read_ptr, packet_end, header);
                break;

            case uvgrtp::frame::RTCP_FT_RR:
                ret = handle_receiver_report_packet(buffer, read_ptr, packet_end, header);
                break;

            case uvgrtp::frame::RTCP_FT_SDES:
                ret = handle_sdes_packet(buffer, read_ptr, packet_end, header, sender_ssrc);
                break;

            case uvgrtp::frame::RTCP_FT_BYE:
                ret = handle_bye_packet(buffer, read_ptr, packet_end, header);
                break;

            case uvgrtp::frame::RTCP_FT_APP:
                ret = handle_app_packet(buffer, read_ptr, packet_end, header);
                break;

            default:
                UVG_LOG_WARN("Unknown packet received, type %d", header.pkt_type);
                break;
        }

        if (ret != RTP_OK)
        {
            UVG_LOG_WARN("Error parsing RTCP packet");
            return ret;
        }

        read_ptr = packet_end;
        remaining_size -= size_of_rtcp_packet;
    }

    if (packets > 1)
    {
        UVG_LOG_DEBUG("Received a compound RTCP frame with %i packets and size: %li", packets, size);
    }
    else
    {
        UVG_LOG_WARN("Received RTCP packet was not a compound packet!");
    }

    return RTP_OK;
}

void uvgrtp::rtcp::read_rtcp_header(const uint8_t* buffer, size_t& read_ptr, uvgrtp::frame::rtcp_header& header)
{
    header.version = (buffer[read_ptr] >> 6) & 0x3;
    header.padding = (buffer[read_ptr] >> 5) & 0x1;

    header.pkt_type = buffer[read_ptr + 1] & 0xff;

    if (header.pkt_type == uvgrtp::frame::RTCP_FT_APP)
    {
        header.pkt_subtype = buffer[read_ptr] & 0x1f;
    }
    else {
        header.count = buffer[read_ptr] & 0x1f;
    }

    header.length = ntohs(*(uint16_t*)& buffer[read_ptr + 2]);

    read_ptr += RTCP_HEADER_SIZE;
}

void uvgrtp::rtcp::read_reports(const uint8_t* buffer, size_t& read_ptr, size_t packet_end, uint8_t count,
    std::vector<uvgrtp::frame::rtcp_report_block>& reports)
{
    for (int i = 0; i < count; ++i)
    {
        if (packet_end >= read_ptr + REPORT_BLOCK_SIZE)
        {
            uvgrtp::frame::rtcp_report_block report;
            report.ssrc = ntohl(*(uint32_t*)& buffer[read_ptr + 0]);
            report.fraction = (ntohl(*(uint32_t*)& buffer[read_ptr + 4])) >> 24;
            report.lost = (ntohl(*(int32_t*)& buffer[read_ptr + 4])) & 0xfffffd;
            report.last_seq = ntohl(*(uint32_t*)& buffer[read_ptr + 8]);
            report.jitter = ntohl(*(uint32_t*)& buffer[read_ptr + 12]);
            report.lsr = ntohl(*(uint32_t*)& buffer[read_ptr + 16]);
            report.dlsr = ntohl(*(uint32_t*)& buffer[read_ptr + 20]);

            reports.push_back(report);
            read_ptr += REPORT_BLOCK_SIZE;
        }
        else {
            UVG_LOG_WARN("Received rtcp packet is smaller than the indicated number of reports!"
                "Read: %i/%i, Read ptr: %i, Packet End:", i, count, read_ptr, packet_end);
        }
    }
}

void uvgrtp::rtcp::read_ssrc(const uint8_t* buffer, size_t& read_ptr, uint32_t& out_ssrc)
{
    out_ssrc = ntohl(*(uint32_t*)& buffer[read_ptr]);
    read_ptr += SSRC_CSRC_SIZE;
}

rtp_error_t uvgrtp::rtcp::handle_receiver_report_packet(uint8_t* buffer, size_t& read_ptr, size_t packet_end,
    uvgrtp::frame::rtcp_header& header)
{
    auto frame = new uvgrtp::frame::rtcp_receiver_report;
    frame->header = header;
    read_ssrc(buffer, read_ptr, frame->ssrc);

    /* Receiver Reports are sent from participant that don't send RTP packets
     * This means that the sender of this report is not in the participants_ map
     * but rather in the initial_participants_ vector
     *
     * Check if that's the case and if so, move the entry from initial_participants_ to participants_ */
    if (!is_participant(frame->ssrc))
    {
        UVG_LOG_INFO("Got an RR from a previously unknown participant SSRC %lu", frame->ssrc);
        add_participant(frame->ssrc);
    }

    read_reports(buffer, read_ptr, packet_end, frame->header.count, frame->report_blocks);

    rr_mutex_.lock();
    if (receiver_hook_) {
        receiver_hook_(frame);
    }
    else if (rr_hook_f_) {
        rr_hook_f_(std::shared_ptr<uvgrtp::frame::rtcp_receiver_report>(frame));
    }
    else if (rr_hook_u_) {
        rr_hook_u_(std::unique_ptr<uvgrtp::frame::rtcp_receiver_report>(frame));
    }
    else {
        std::lock_guard prtcp_lock(participants_mutex_);
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

rtp_error_t uvgrtp::rtcp::handle_sender_report_packet(uint8_t* buffer, size_t& read_ptr, size_t packet_end,
    uvgrtp::frame::rtcp_header& header)
{
    auto frame = new uvgrtp::frame::rtcp_sender_report;
    frame->header = header;
    read_ssrc(buffer, read_ptr, frame->ssrc);
    if (!is_participant(frame->ssrc))
    {
        UVG_LOG_INFO("Got an SR from a previously unknown participant SSRC %lu", frame->ssrc);
        add_participant(frame->ssrc);
    }

    participants_mutex_.lock();
    participants_[frame->ssrc]->stats.sr_ts = uvgrtp::clock::hrc::now();

    frame->sender_info.ntp_msw = ntohl(*(uint32_t*)& buffer[read_ptr]);
    frame->sender_info.ntp_lsw = ntohl(*(uint32_t*)& buffer[read_ptr + 4]);
    frame->sender_info.rtp_ts = ntohl(*(uint32_t*)& buffer[read_ptr + 8]);
    frame->sender_info.pkt_cnt = ntohl(*(uint32_t*)& buffer[read_ptr + 12]);
    frame->sender_info.byte_cnt = ntohl(*(uint32_t*)& buffer[read_ptr + 16]);
    read_ptr += SENDER_INFO_SIZE;

    participants_[frame->ssrc]->stats.lsr =
        ((frame->sender_info.ntp_msw & 0xffff) << 16) |
        (frame->sender_info.ntp_lsw >> 16);
    participants_mutex_.unlock();

    read_reports(buffer, read_ptr, packet_end, frame->header.count, frame->report_blocks);

    sr_mutex_.lock();
    if (sender_hook_) {
        sender_hook_(frame);
    }
    else if (sr_hook_f_) {
        sr_hook_f_(std::shared_ptr<uvgrtp::frame::rtcp_sender_report>(frame));
    }
    else if (sr_hook_u_) {
        sr_hook_u_(std::unique_ptr<uvgrtp::frame::rtcp_sender_report>(frame));
    }
    else {
        std::lock_guard prtcp_lock(participants_mutex_);
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


rtp_error_t uvgrtp::rtcp::handle_sdes_packet(uint8_t* packet, size_t& read_ptr, size_t packet_end,
    uvgrtp::frame::rtcp_header& header, uint32_t sender_ssrc)
{
    if (!is_participant(sender_ssrc))
    {
        UVG_LOG_INFO("Got an SDES packet from a previously unknown participant SSRC %lu", sender_ssrc);
        add_participant(sender_ssrc);
    }

    auto frame = new uvgrtp::frame::rtcp_sdes_packet;
    frame->header = header;

    // Read SDES chunks
    while (read_ptr + SSRC_CSRC_SIZE <= packet_end)
    {
        uvgrtp::frame::rtcp_sdes_chunk chunk;
        read_ssrc(packet, read_ptr, chunk.ssrc);

        // Read chunk items, 2 makes sure we at least get item type and length
        while (read_ptr + 2 <= packet_end && packet[read_ptr] != 0)
        {
            uvgrtp::frame::rtcp_sdes_item item;
            item.type = packet[read_ptr];
            item.length = packet[read_ptr + 1];
            read_ptr += 2;

            if (read_ptr + item.length <= packet_end)
            {
                item.data = new uint8_t[item.length];

                memcpy(item.data, &packet[read_ptr], item.length);
                read_ptr += item.length;
            }

            chunk.items.push_back(item);
        }

        if (packet[read_ptr] == 0)
        {
            read_ptr += (4 - read_ptr % 4);
        }

        frame->chunks.push_back(chunk);
    }

    sdes_mutex_.lock();
    if (sdes_hook_) {
        sdes_hook_(frame);
    } else if (sdes_hook_f_) {
        sdes_hook_f_(std::shared_ptr<uvgrtp::frame::rtcp_sdes_packet>(frame));
    } else if (sdes_hook_u_) {
        sdes_hook_u_(std::unique_ptr<uvgrtp::frame::rtcp_sdes_packet>(frame));
    } else {
        std::lock_guard prtcp_lock(participants_mutex_);
        // Deallocate previous frame from the buffer if it exists, it's going to get overwritten
        if (participants_[sender_ssrc]->sdes_frame)
        {
            for (auto& chunk : participants_[sender_ssrc]->sdes_frame->chunks)
            {
                for (auto& item : chunk.items)
                {
                    delete[](uint8_t*)item.data;
                    item.data = nullptr;
                }
            }
            delete participants_[sender_ssrc]->sdes_frame;
            participants_[sender_ssrc]->sdes_frame = nullptr;
        }

        participants_[sender_ssrc]->sdes_frame = frame;
    }
    sdes_mutex_.unlock();


    return RTP_OK;
}

rtp_error_t uvgrtp::rtcp::handle_bye_packet(uint8_t* packet, size_t& read_ptr, 
    size_t packet_end, uvgrtp::frame::rtcp_header& header)
{
    (void)header; // TODO: Process BYE packet better

    for (size_t i = 0; i < packet_end; i += sizeof(uint32_t))
    {
        uint32_t ssrc = 0; 
        read_ssrc(packet, read_ptr, ssrc);

        if (!is_participant(ssrc))
        {
            UVG_LOG_WARN("Participant %lu is not part of this group!", ssrc);
            continue;
        }

        UVG_LOG_DEBUG("Destroying participant with BYE");

        participants_mutex_.lock();
        free_participant(std::move(participants_[ssrc]));
        participants_.erase(ssrc);
        participants_mutex_.unlock();
    }

    // TODO: Give BYE packet to user and read optional reason for BYE

    return RTP_OK;
}

rtp_error_t uvgrtp::rtcp::handle_app_packet(uint8_t* packet, size_t& read_ptr,
    size_t packet_end, uvgrtp::frame::rtcp_header& header)
{
    auto frame = new uvgrtp::frame::rtcp_app_packet;
    frame->header = header;
    read_ssrc(packet, read_ptr, frame->ssrc);

    /* Deallocate previous frame from the buffer if it exists, it's going to get overwritten */
    if (!is_participant(frame->ssrc))
    {
        UVG_LOG_WARN("Got an APP packet from an unknown participant");
        add_participant(frame->ssrc);
    }

    // copy app name and application-dependent data from network packet to RTCP structures
    memcpy(frame->name, &packet[read_ptr], APP_NAME_SIZE);
    read_ptr += APP_NAME_SIZE;

    frame->payload_len = packet_end - read_ptr;

    if (frame->payload_len > 0)
    {
        // application data is saved to payload
        frame->payload = new uint8_t[frame->payload_len];
        memcpy(frame->payload, &packet[read_ptr], frame->payload_len);
    }
    else
    {
        frame->payload = nullptr;
    }

    app_mutex_.lock();
    if (app_hook_) {
        app_hook_(frame);
    } else if (app_hook_f_) {
        app_hook_f_(std::shared_ptr<uvgrtp::frame::rtcp_app_packet>(frame));
    } else if (app_hook_u_) {
        app_hook_u_(std::unique_ptr<uvgrtp::frame::rtcp_app_packet>(frame));
    } else {
        std::lock_guard prtcp_lock(participants_mutex_);
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

rtp_error_t uvgrtp::rtcp::send_rtcp_packet_to_participants(uint8_t* frame, uint32_t frame_size, bool encrypt)
{
    if (!frame)
    {
        UVG_LOG_ERROR("No frame given for sending");
        return RTP_GENERIC_ERROR;
    }

    rtp_error_t ret = RTP_OK;

    if (encrypt && srtcp_ && 
        (ret = srtcp_->handle_rtcp_encryption(rce_flags_, rtcp_pkt_sent_count_, *ssrc_.get(), frame, frame_size)) != RTP_OK)
    {
        UVG_LOG_DEBUG("Encryption failed. Not sending packet");
        delete[] frame;
        return ret;
    }

    for (auto& p : participants_)
    {
        std::lock_guard prtcp_lock(participants_mutex_);
        if (p.second->socket != nullptr)
        {
            if ((ret = p.second->socket->sendto(p.second->address, frame, frame_size, 0)) != RTP_OK)
            {
                UVG_LOG_ERROR("Sending rtcp packet with sendto() failed!");
                break;
            }

            update_rtcp_bandwidth(frame_size);
        }
        else
        {
            UVG_LOG_ERROR("Tried to send RTCP packet when socket does not exist!");
        }
    }
    delete[] frame;
    return ret;
}

uint32_t uvgrtp::rtcp::size_of_ready_app_packets() const
{
    uint32_t app_size = 0;
    for (auto& app_name : app_packets_)
    {
        // TODO: Should we also send one per subtype?
        if (!app_name.second.empty())
        {
            app_size += get_app_packet_size(app_name.second.front().payload_len);
        }
    }

    return app_size;
}

uint32_t uvgrtp::rtcp::size_of_apps_from_hook(std::vector<std::shared_ptr<rtcp_app_packet>> packets) const
{
    uint32_t app_size = 0;
    for (auto& pkt : packets)
    {
        if (pkt) app_size += get_app_packet_size(pkt->payload_len);
    }
    return app_size;
}

uint32_t uvgrtp::rtcp::size_of_compound_packet(uint16_t reports,
    bool sr_packet, bool rr_packet, bool sdes_packet, uint32_t app_size, bool bye_packet) const
{
    uint32_t compound_packet_size = 0;

    if (sr_packet)
    {  
        compound_packet_size = get_sr_packet_size(rce_flags_, reports);
        UVG_LOG_DEBUG("Sending SR. Compound packet size: %li", compound_packet_size);
    }
    else if (rr_packet)
    {
        compound_packet_size = get_rr_packet_size(rce_flags_, reports);
        UVG_LOG_DEBUG("Sending RR. Compound packet size: %li", compound_packet_size);
    }
    else
    {
        UVG_LOG_ERROR("RTCP compound packet must start with either SR or RR");
        return 0;
    }

    if (sdes_packet)
    {
        compound_packet_size += get_sdes_packet_size(ourItems_);
        UVG_LOG_DEBUG("Sending SDES. Compound packet size: %li", compound_packet_size);
    }

    if (app_size != 0)
    {
        compound_packet_size += app_size;
        UVG_LOG_DEBUG("Sending APP. Compound packet size: %li", compound_packet_size);
    }

    if (bye_packet)
    {
        compound_packet_size += get_bye_packet_size(bye_ssrcs_);
        UVG_LOG_DEBUG("Sending BYE. Compound packet size: %li", compound_packet_size);
    }

    return compound_packet_size;
}

rtp_error_t uvgrtp::rtcp::generate_report()
{
    std::lock_guard<std::mutex> lock(packet_mutex_);
    rtcp_pkt_sent_count_++;

    bool sr_packet = our_role_ == SENDER && our_stats.sent_rtp_packet;
    bool rr_packet = our_role_ == RECEIVER || our_stats.sent_rtp_packet == 0;
    bool sdes_packet = true;
    uint32_t app_packets_size = size_of_ready_app_packets();
    bool bye_packet = !bye_ssrcs_.empty();

    // Unique lock unlocks when exiting the scope
    std::unique_lock prtcp_lock(participants_mutex_);
    uint8_t reports = 0;
    for (auto& p : participants_)
    {
        if (p.second->stats.received_rtp_packet)
        {
            ++reports;
        }
    }
    std::vector< std::shared_ptr<rtcp_app_packet>> outgoing_apps_;
    if (hooked_app_) {
        std::lock_guard<std::mutex> grd(send_app_mutex_);
        for (auto& [name, hook] : outgoing_app_hooks_) {
            uint32_t p_len = 0; 
            uint8_t subtype = 0; 
            std::vector<uint8_t> pload = {};
            
            hook(p_len, subtype, pload);
            if (p_len > 0 && !pload.empty()) {
                std::shared_ptr<rtcp_app_packet> app_pkt = std::make_shared<rtcp_app_packet>(name.data(), subtype, p_len, pload.data());
                outgoing_apps_.push_back(app_pkt);
            }
        }
        app_packets_size = size_of_apps_from_hook(outgoing_apps_);
    }
    uint32_t compound_packet_size = size_of_compound_packet(reports, sr_packet, rr_packet, sdes_packet, app_packets_size, bye_packet);
    
    if (compound_packet_size == 0)
    {
        UVG_LOG_WARN("Failed to get compound packet size");
        return RTP_GENERIC_ERROR;
    }
    else if (compound_packet_size > mtu_size_)
    {
        UVG_LOG_WARN("Generate RTCP packet is too large %lli/%lli, reports should be circled, but not implemented!",
            compound_packet_size, mtu_size_);
    }

    uint8_t* frame = new uint8_t[compound_packet_size];
    memset(frame, 0, compound_packet_size);

    // see https://datatracker.ietf.org/doc/html/rfc3550#section-6.4.1

    size_t write_ptr = 0;
    uint32_t ssrc = *ssrc_.get();
    if (sr_packet)
    {
        // sender reports have sender information in addition compared to receiver reports
        size_t sender_report_size = get_sr_packet_size(rce_flags_, reports);

        /* TODO: The clock would be better to start with first sent RTP packet.
         * In reality it should be provided by user which I think is implemented? */
        if (clock_start_ == 0)
        {
          clock_start_ = uvgrtp::clock::ntp::now();
        }

        /* TODO: The RTP timestamp should be from an actual RTP packet and NTP timestamp should be the one
           corresponding to it. */
        uint64_t ntp_ts = uvgrtp::clock::ntp::now();

        uint64_t expanded_ts_start = rtp_ts_start_;
        uint64_t ts_since_start = (uint64_t)(uvgrtp::clock::ntp::diff(clock_start_, ntp_ts) * double(clock_rate_ / 1000));

        uint64_t rtp_ts = expanded_ts_start + ts_since_start;

        if (!construct_rtcp_header(frame, write_ptr, sender_report_size, reports, uvgrtp::frame::RTCP_FT_SR) ||
            !construct_ssrc(frame, write_ptr, ssrc) ||
            !construct_sender_info(frame, write_ptr, ntp_ts, rtp_ts, our_stats.sent_pkts, our_stats.sent_bytes))
        {
            UVG_LOG_ERROR("Failed to construct SR");
            return RTP_GENERIC_ERROR;
        }

        our_stats.sent_rtp_packet = false;

    } else if (rr_packet) { // RECEIVER
        size_t receiver_report_size = get_rr_packet_size(rce_flags_, reports);

        if (!construct_rtcp_header(frame, write_ptr, receiver_report_size, reports, uvgrtp::frame::RTCP_FT_RR) ||
            !construct_ssrc(frame, write_ptr, ssrc))
        {
            UVG_LOG_ERROR("Failed to construct RR");
            return RTP_GENERIC_ERROR;
        }
    }
    else
    {
        // SR or RR is mandatory at the beginning
        return RTP_GENERIC_ERROR;
    }

    // the report blocks for sender or receiver report. Both have same reports.
    for (auto& p : participants_)
    {
        // only add report blocks if we have received data from them
        if (p.second->stats.received_rtp_packet)
        {
            uint32_t dropped_packets = p.second->stats.dropped_pkts;

            // TODO: Fraction should be the number of packets lost compared to number of packets expected (see fraction lost in RFC 3550)
            // see https://datatracker.ietf.org/doc/html/rfc3550#appendix-A.3
            
            //uint8_t fraction = dropped_packets ? p.second->stats.received_bytes / dropped_packets : 0;
            uint8_t fraction = 0; // disabled, because it was incorrect

            uint64_t diff = (u_long)uvgrtp::clock::hrc::diff_now(p.second->stats.sr_ts);
            uint32_t dlrs = (uint32_t)uvgrtp::clock::ms_to_jiffies(diff);

            /* calculate delay of last SR only if SR has been received at least once */
            if (p.second->stats.lsr == 0)
            {
                dlrs = 0;
            }

            construct_report_block(frame, write_ptr, p.first, fraction, dropped_packets,
                p.second->stats.cycles, p.second->stats.max_seq, (uint32_t)p.second->stats.jitter, 
                p.second->stats.lsr, dlrs);

            // we only send reports if there is something to report since last report
            p.second->stats.received_rtp_packet = false;
        }
    }
    prtcp_lock.unlock(); // End of critical section involving participants_

    if (sdes_packet)
    {
        uvgrtp::frame::rtcp_sdes_chunk chunk;
        chunk.items = ourItems_;
        chunk.ssrc = *ssrc_.get();

        // add the SDES packet after the SR/RR, mandatory, must contain CNAME
        if (!construct_rtcp_header(frame, write_ptr, get_sdes_packet_size(ourItems_), num_receivers_,
            uvgrtp::frame::RTCP_FT_SDES) ||
            !construct_sdes_chunk(frame, write_ptr, chunk))
        {
            UVG_LOG_ERROR("Failed to add SDES packet");
            delete[] frame;
            return RTP_GENERIC_ERROR;
        }
    }

    if (app_packets_size != 0)
    {
        if (hooked_app_) {
            for (auto& pkt : outgoing_apps_) {
                if(!construct_app_block(frame, write_ptr, pkt->subtype & 0x1f, *ssrc_.get(), pkt->name, pkt->payload, pkt->payload_len))
                {
                    UVG_LOG_ERROR("Failed to construct APP packet");
                    delete[] frame;
                    return RTP_GENERIC_ERROR;
                }
            }
        }
        else {
            for (auto& app_name : app_packets_)
            {
                // TODO: Should we also send one per subtype?
                if (!app_name.second.empty())
                {
                    // take the oldest APP packet and send it
                    rtcp_app_packet& next_packet = app_name.second.front();
                    if (!construct_app_block(frame, write_ptr, next_packet.subtype & 0x1f, *ssrc_.get(), next_packet.name, next_packet.payload, next_packet.payload_len))
                    {
                        UVG_LOG_ERROR("Failed to construct APP packet");
                        delete[] frame;
                        app_name.second.pop_front();
                        return RTP_GENERIC_ERROR;
                    }
                    app_name.second.pop_front();
                }
            }
        }
    }

    // BYE is last if it is sent
    if (bye_packet)
    {
        // header construction does not add our ssrc for BYE
        uint8_t secondField = (bye_ssrcs_.size() & 0x1f);
        if (!construct_rtcp_header(frame, write_ptr, get_bye_packet_size(bye_ssrcs_), secondField,
            uvgrtp::frame::RTCP_FT_BYE) ||
            !construct_bye_packet(frame, write_ptr, bye_ssrcs_))
        {
            bye_ssrcs_.clear();
            UVG_LOG_ERROR("Failed to construct BYE");
            delete[] frame;
            return RTP_GENERIC_ERROR;
        }

        bye_ssrcs_.clear();
    }
    

    UVG_LOG_DEBUG("Sending RTCP report compound packet, Total size: %lli",
        compound_packet_size);

    return send_rtcp_packet_to_participants(frame, compound_packet_size, true);
}

rtp_error_t uvgrtp::rtcp::send_sdes_packet(const std::vector<uvgrtp::frame::rtcp_sdes_item>& items)
{
    if (items.empty())
    {
        UVG_LOG_ERROR("Cannot send an empty SDES packet!");
        return RTP_INVALID_VALUE;
    }

    packet_mutex_.lock();
    rtp_error_t ret = set_sdes_items(items);
    packet_mutex_.unlock();

    return ret;
}

rtp_error_t uvgrtp::rtcp::send_bye_packet(std::vector<uint32_t> ssrcs)
{
    // ssrcs contains all our ssrcs which we usually have one unless we are a mixer
    if (ssrcs.empty())
    {
        UVG_LOG_WARN("Source Count in RTCP BYE packet is 0. Not sending.");
    }
    packet_mutex_.lock();
    bye_ssrcs_ = ssrcs;
    packet_mutex_.unlock();

    return RTP_OK;
}

rtp_error_t uvgrtp::rtcp::send_app_packet(const char* name, uint8_t subtype,
    uint32_t payload_len, const uint8_t* payload)
{
    packet_mutex_.lock();
    if (!app_packets_[name].empty())
    {
        UVG_LOG_DEBUG("Adding a new APP packet for sending when %llu packets are waiting to be sent",
            app_packets_[name].size());
    }
    app_packets_[name].emplace_back(name, subtype, payload_len, payload);
    packet_mutex_.unlock();

    return RTP_OK;
}

uint32_t uvgrtp::rtcp::get_rtcp_interval_ms() const 
{
    return interval_ms_;
}

void uvgrtp::rtcp::set_session_bandwidth(uint32_t kbps)
{
    interval_ms_ = 1000*360 / kbps; // the reduced minimum (see section 6.2 in RFC 3550)

    if (interval_ms_ > DEFAULT_RTCP_INTERVAL_MS)
    {
        interval_ms_ = DEFAULT_RTCP_INTERVAL_MS;
    }
    // TODO: This should follow the algorithm specified in RFC 3550 appendix-A.7
}

void uvgrtp::rtcp::set_payload_size(size_t mtu_size)
{
    mtu_size_ = mtu_size;
}
