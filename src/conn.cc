#ifdef __linux__
#include <arpa/inet.h>
#include <unistd.h>
#endif

#include <cassert>
#include <cstring>
#include <iostream>

#include "conn.hh"
#include "debug.hh"
#include "queue.hh"
#include "random.hh"
#include "util.hh"

#include "formats/hevc.hh"
#include "formats/opus.hh"

kvz_rtp::connection::connection(rtp_format_t fmt, bool reader):
    config_(nullptr),
    socket_(),
    rtcp_(nullptr),
    reader_(reader),
    wc_start_(0),
    fqueue_(nullptr)
{
    rtp_sequence_  = 1; //kvz_rtp::random::generate_32();
    rtp_ssrc_      = kvz_rtp::random::generate_32();
    rtp_payload_   = fmt;

    if (!reader)
        fqueue_ = new kvz_rtp::frame_queue(fmt);
}

kvz_rtp::connection::~connection()
{
    if (rtcp_) {
        rtcp_->terminate();
        delete rtcp_;
    }
}

void kvz_rtp::connection::set_payload(rtp_format_t fmt)
{
    rtp_payload_ = fmt;

    switch (fmt) {
        case RTP_FORMAT_HEVC:
            clock_rate_ = 90000;
            break;

        case RTP_FORMAT_OPUS:
            clock_rate_ = 48000;
            break;

        default:
            LOG_WARN("Unknown RTP format, clock rate must be set manually");
            break;
    }
}

void kvz_rtp::connection::set_config(void *config)
{
    config_ = config;
}

void *kvz_rtp::connection::get_config()
{
    return config_;
}

uint16_t kvz_rtp::connection::get_sequence() const
{
    return rtp_sequence_;
}

uint32_t kvz_rtp::connection::get_ssrc() const
{
    return rtp_ssrc_;
}

void kvz_rtp::connection::set_ssrc(uint32_t ssrc)
{
    rtp_ssrc_ = ssrc;
}

uint8_t kvz_rtp::connection::get_payload() const
{
    return rtp_payload_;
}

kvz_rtp::socket_t kvz_rtp::connection::get_raw_socket()
{
    return socket_.get_raw_socket();
}

kvz_rtp::socket& kvz_rtp::connection::get_socket()
{
    return socket_;
}

void kvz_rtp::connection::inc_rtp_sequence(size_t n)
{
    rtp_sequence_ += n;
}

void kvz_rtp::connection::inc_rtp_sequence()
{
    rtp_sequence_++;

    if (rtp_sequence_ == 0 && rtcp_)
        rtcp_->sender_inc_seq_cycle_count();
}

void kvz_rtp::connection::inc_sent_bytes(size_t n)
{
    if (rtcp_)
        rtcp_->sender_inc_sent_bytes(n);
}

void kvz_rtp::connection::inc_sent_pkts(size_t n)
{
    if (rtcp_)
        rtcp_->sender_inc_sent_pkts(n);
}

void kvz_rtp::connection::inc_sent_pkts()
{
    if (rtcp_)
        rtcp_->sender_inc_sent_pkts(1);
}

rtp_error_t kvz_rtp::connection::update_receiver_stats(kvz_rtp::frame::rtp_frame *frame)
{
    rtp_error_t ret;

    if (rtcp_) {
        if ((ret = rtcp_->receiver_update_stats(frame)) != RTP_SSRC_COLLISION)
            return ret;

        do {
            rtp_ssrc_ = kvz_rtp::random::generate_32();
        } while ((rtcp_->reset_rtcp_state(rtp_ssrc_)) != RTP_OK);

        /* even though we've resolved the SSRC conflict, we still need to return an error
         * code because the original packet that caused the conflict is considered "invalid" */
        return RTP_INVALID_VALUE;
    }

    return RTP_OK;
}

void kvz_rtp::connection::set_clock_rate(uint32_t clock_rate)
{
    clock_rate_ = clock_rate;
}

void kvz_rtp::connection::fill_rtp_header(uint8_t *buffer)
{
    if (!buffer)
        return;

    /* This is the first RTP message, get wall clock reading (t = 0)
     * and generate random RTP timestamp for this reading */
    if (wc_start_ == 0) {
        rtp_timestamp_ = kvz_rtp::random::generate_32();
        wc_start_      = kvz_rtp::clock::ntp::now();

        if (rtcp_)
            rtcp_->set_sender_ts_info(wc_start_, clock_rate_, rtp_timestamp_);
    }

    buffer[0] = 2 << 6; // RTP version
    buffer[1] = (rtp_payload_ & 0x7f) | (0 << 7);

    *(uint16_t *)&buffer[2] = htons(rtp_sequence_);
    *(uint32_t *)&buffer[4] = htonl(
        rtp_timestamp_
        + kvz_rtp::clock::ntp::diff_now(wc_start_)
        * clock_rate_
        / 1000
    );
    *(uint32_t *)&buffer[8] = htonl(rtp_ssrc_);
}

void kvz_rtp::connection::update_rtp_sequence(uint8_t *buffer)
{
    if (!buffer)
        return;

    *(uint16_t *)&buffer[2] = htons(rtp_sequence_);
}

rtp_error_t kvz_rtp::connection::create_rtcp(std::string dst_addr, int dst_port, int src_port)
{
    if (rtcp_ == nullptr) {
        if ((rtcp_ = new kvz_rtp::rtcp(get_ssrc(), reader_)) == nullptr) {
            LOG_ERROR("Failed to allocate RTCP instance!");
            return RTP_MEMORY_ERROR;
        }
    }

    if ((rtp_errno = rtcp_->add_participant(dst_addr, dst_port, src_port, clock_rate_)) != RTP_OK) {
        LOG_ERROR("Failed to add RTCP participant!");
        return rtp_errno;
    }

    return rtcp_->start();
}

kvz_rtp::frame_queue *kvz_rtp::connection::get_frame_queue()
{
    return fqueue_;
}
