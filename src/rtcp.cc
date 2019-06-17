#ifdef _WIN32
#include <windows.h>
#else
#include <sys/time.h>
#endif

#include <iostream>

#include "debug.hh"
#include "rtcp.hh"
#include "util.hh"

kvz_rtp::rtcp::rtcp():
    tp_(0), tc_(0), tn_(0), pmembers_(0),
    members_(0), senders_(0), rtcp_bandwidth_(0),
    we_sent_(0), avg_rtcp_pkt_pize_(0), initial_(true),
    active_(false), send_addr_(""), send_port_(0), recv_port_(0),
    socket_(), num_receivers_(0)
{
    cname_ = "hello"; //generate_cname();
}

kvz_rtp::rtcp::rtcp(std::string dst_addr, int dst_port, bool receiver):
    rtcp()
{
    send_addr_ = dst_addr;
    send_port_ = dst_port;
    receiver_  = receiver;
}

kvz_rtp::rtcp::rtcp(std::string dst_addr, int dst_port, int src_port, bool receiver):
    rtcp(dst_addr, dst_port, receiver)
{
    recv_port_ = src_port;
}

kvz_rtp::rtcp::~rtcp()
{
}

rtp_error_t kvz_rtp::rtcp::start()
{
    if (send_addr_ == "" || send_port_ == 0) {
        LOG_ERROR("Invalid values given (%s, %d), cannot create RTCP instance", send_addr_.c_str(), send_port_);
        return RTP_INVALID_VALUE;
    }

    rtp_error_t ret;

    if ((ret = socket_.init(AF_INET, SOCK_DGRAM, 0)) != RTP_OK)
        return ret;

    /* if the receive port was given, this RTCP instance
     * is expecting status reports to that port and should bind the socket to it */
    if (recv_port_ != 0) {
        int enable = 1;

        if ((ret = socket_.setsockopt(SOL_SOCKET, SO_REUSEADDR, (const char *)&enable, sizeof(int))) != RTP_OK)
            return ret;

        /* Set read timeout (5s for now) 
         *
         * TODO: this doesn't btw work...
         *
         * This means that the socket is listened for 5s at a time and after the timeout, 
         * Send Report is sent to all participants */
        struct timeval tv;
        tv.tv_sec = 3;
        tv.tv_usec = 0;

        if ((ret = socket_.setsockopt(SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv))) != RTP_OK)
            return ret;

        LOG_DEBUG("Binding to port %d (source port)", recv_port_);

        if ((ret = socket_.bind(AF_INET, INADDR_ANY, recv_port_)) != RTP_OK)
            return ret;
    }

    socket_.set_sockaddr(socket_.create_sockaddr(AF_INET, send_addr_, send_port_));

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
    for (auto& i : receiver_stats_) {
        delete i.second;
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

const kvz_rtp::socket& kvz_rtp::rtcp::get_socket() const
{
    return socket_;
}

void kvz_rtp::rtcp::sender_inc_processed_bytes(size_t n)
{
    sender_stats_.processed_bytes += n;
}

void kvz_rtp::rtcp::sender_inc_overhead_bytes(size_t n)
{
    sender_stats_.overhead_bytes += n;
}

void kvz_rtp::rtcp::sender_inc_total_bytes(size_t n)
{
    sender_stats_.total_bytes += n;
}

void kvz_rtp::rtcp::sender_inc_processed_pkts(size_t n)
{
    sender_stats_.processed_pkts += n;
}

void kvz_rtp::rtcp::check_sender(uint32_t ssrc)
{
    if (receiver_stats_.find(ssrc) == receiver_stats_.end()) {
        LOG_INFO("First RTP packet from 0x%x", ssrc);

        receiver_stats_[ssrc] = new struct statistics;
        num_receivers_++;
    }
}

void kvz_rtp::rtcp::receiver_inc_processed_bytes(uint32_t sender_ssrc, size_t n)
{
    check_sender(sender_ssrc);

    receiver_stats_[sender_ssrc]->processed_bytes += n;
}

void kvz_rtp::rtcp::receiver_inc_overhead_bytes(uint32_t sender_ssrc, size_t n)
{
    check_sender(sender_ssrc);

    receiver_stats_[sender_ssrc]->overhead_bytes += n;
}

void kvz_rtp::rtcp::receiver_inc_total_bytes(uint32_t sender_ssrc, size_t n)
{
    check_sender(sender_ssrc);

    receiver_stats_[sender_ssrc]->total_bytes += n;
}

void kvz_rtp::rtcp::receiver_inc_processed_pkts(uint32_t sender_ssrc, size_t n)
{
    check_sender(sender_ssrc);

    receiver_stats_[sender_ssrc]->processed_pkts += n;
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
    rtp_error_t ret    = RTP_OK;
    uint64_t timestamp = tv_to_ntp();

    frame->s_info.ntp_msw  = timestamp >> 32;
    frame->s_info.ntp_lsw  = timestamp & 0xffffffff;
    frame->s_info.rtp_ts   = 3; /* TODO: what timestamp is this? */
    frame->s_info.pkt_cnt  = sender_stats_.processed_pkts;
    frame->s_info.byte_cnt = sender_stats_.processed_bytes;

    for (auto& receiver : receiver_stats_) {
        frame->blocks[ptr].ssrc = receiver.first;

        if (receiver.second->dropped_pkts) {
            frame->blocks[ptr].fraction_lost =
                receiver.second->processed_pkts / receiver.second->dropped_pkts;
        }

        frame->blocks[ptr].cumulative_pkt_lost = receiver.second->dropped_pkts;
        frame->blocks[ptr].highest_seq_recved  = 222; // TODO ???
        frame->blocks[ptr].interraival_jitter  = 333; // TODO ???
        frame->blocks[ptr].delay_since_last_sr = 555; // TODO ???
        frame->blocks[ptr].last_sr             = 444; // TODO ???

        ptr++;
    }

    ret = socket_.sendto((uint8_t *)frame, frame->header.length, 0, NULL);
    kvz_rtp::frame::dealloc_frame(frame);

    return ret;
}

rtp_error_t kvz_rtp::rtcp::generate_receiver_report()
{
    LOG_INFO("Generating receiver report...");

    kvz_rtp::frame::rtcp_receiver_frame *frame;

    if ((frame = kvz_rtp::frame::alloc_rtcp_receiver_frame(num_receivers_)) == nullptr) {
        LOG_ERROR("Failed to allocate RTCP Receiver Report frame!");
        return rtp_errno;
    }

    rtp_error_t ret = RTP_OK;
    size_t ptr      = 0;

    for (auto& receiver : receiver_stats_) {
        frame->blocks[ptr].ssrc = receiver.first;

        if (receiver.second->dropped_pkts) {
            frame->blocks[ptr].fraction_lost =
                receiver.second->processed_pkts / receiver.second->dropped_pkts;
        }

        frame->blocks[ptr].cumulative_pkt_lost = receiver.second->dropped_pkts;
        frame->blocks[ptr].highest_seq_recved  = 222; // TODO ???
        frame->blocks[ptr].interraival_jitter  = 333; // TODO ???
        frame->blocks[ptr].delay_since_last_sr = 555; // TODO ???
        frame->blocks[ptr].last_sr             = 444; // TODO ???

        ptr++;
    }

    ret = socket_.sendto((uint8_t *)frame, frame->header.length, 0, NULL);
    kvz_rtp::frame::dealloc_frame(frame);

    return ret;
}

rtp_error_t kvz_rtp::rtcp::generate_report()
{
    if (receiver_)
        return generate_receiver_report();
    return generate_sender_report();
}

rtp_error_t kvz_rtp::rtcp::handle_sender_report_packet(kvz_rtp::frame::rtcp_sender_frame *report)
{
    if (!report)
        return RTP_INVALID_VALUE;

    if (report->header.report_cnt == 0) {
        LOG_ERROR("Receiver report cannot have 0 report blocks!");
        return RTP_INVALID_VALUE;
    }

    /* TODO: what are we supposed to with this report? */

    return RTP_OK;
}

rtp_error_t kvz_rtp::rtcp::handle_receiver_report_packet(kvz_rtp::frame::rtcp_receiver_frame *report)
{
    if (!report)
        return RTP_INVALID_VALUE;

    if (report->header.report_cnt == 0) {
        LOG_ERROR("Receiver report cannot have 0 report blocks!");
        return RTP_INVALID_VALUE;
    }

    /* TODO: what are we supposed to with this report? */

    return RTP_OK;
}

rtp_error_t kvz_rtp::rtcp::handle_sdes_packet(kvz_rtp::frame::rtcp_sdes_frame *sdes)
{
    (void)sdes;
    return RTP_OK;
}

rtp_error_t kvz_rtp::rtcp::handle_bye_packet(kvz_rtp::frame::rtcp_bye_frame *bye)
{
    (void)bye;
    return RTP_OK;
}

rtp_error_t kvz_rtp::rtcp::handle_app_packet(kvz_rtp::frame::rtcp_app_frame *app)
{
    (void)app;
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

    rtp_error_t ret;

    switch (header->pkt_type) {
        case kvz_rtp::frame::FRAME_TYPE_SR:
            ret = handle_sender_report_packet((kvz_rtp::frame::rtcp_sender_frame *)buffer);
            break;

        case kvz_rtp::frame::FRAME_TYPE_RR:
            ret = handle_receiver_report_packet((kvz_rtp::frame::rtcp_receiver_frame *)buffer);
            break;

        case kvz_rtp::frame::FRAME_TYPE_SDES:
            /* ret = handle_sender_packet((kvz_rtp::frame::rtcp_sender_frame *)buffer); */
            break;

        case kvz_rtp::frame::FRAME_TYPE_BYE:
            /* ret = handle_sender_packet((kvz_rtp::frame::rtcp_sender_frame *)buffer); */
            break;

        case kvz_rtp::frame::FRAME_TYPE_APP:
            /* ret = handle_app_packet((kvz_rtp::frame::rtcp_sender_frame *)buffer); */
            break;
    }

    return ret;
}

void kvz_rtp::rtcp::rtcp_runner(kvz_rtp::rtcp *rtcp)
{
    LOG_INFO("RTCP instance created!");

    int nread;
    uint8_t buffer[MAX_PACKET];

    kvz_rtp::socket socket = rtcp->get_socket();
    rtp_error_t ret;

    while (rtcp->active()) {
        ret = socket.recvfrom(buffer, MAX_PACKET, 0, &nread);

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

    /* TODO: send bye */
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
