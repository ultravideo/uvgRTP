#ifdef __linux__
#include <arpa/inet.h>
#include <unistd.h>
#endif

#include <chrono>

#include "clock.hh"
#include "debug.hh"
#include "random.hh"
#include "rtp.hh"

#define INVALID_TS UINT64_MAX

uvg_rtp::rtp::rtp(rtp_format_t fmt):
    wc_start_(0),
    sent_pkts_(0),
    timestamp_(INVALID_TS),
    payload_size_(MAX_PAYLOAD)
{
    seq_  = uvg_rtp::random::generate_32() & 0xffff;
    ts_   = uvg_rtp::random::generate_32();
    ssrc_ = uvg_rtp::random::generate_32();

    set_payload(fmt);
}

uvg_rtp::rtp::~rtp()
{
}

uint32_t uvg_rtp::rtp::get_ssrc()
{
    return ssrc_;
}

uint16_t uvg_rtp::rtp::get_sequence()
{
    return seq_;
}

void uvg_rtp::rtp::set_payload(rtp_format_t fmt)
{
    payload_ = fmt_ = fmt;

    switch (fmt_) {
        case RTP_FORMAT_HEVC:
            clock_rate_ = 90000;
            break;

        case RTP_FORMAT_OPUS:
            clock_rate_ = 48000;
            break;

        default:
            LOG_WARN("Unknown RTP format, setting clock rate to 10000");
            clock_rate_ = 10000;
            break;
    }
}

void uvg_rtp::rtp::set_dynamic_payload(uint8_t payload)
{
    payload_ = payload;
}

void uvg_rtp::rtp::inc_sequence()
{
    seq_++;
}

void uvg_rtp::rtp::inc_sent_pkts()
{
    sent_pkts_++;
}

void uvg_rtp::rtp::update_sequence(uint8_t *buffer)
{
    if (!buffer)
        return;

    *(uint16_t *)&buffer[2] = htons(seq_);
}

void uvg_rtp::rtp::fill_header(uint8_t *buffer)
{
    if (!buffer)
        return;

    /* This is the first RTP message, get wall clock reading (t = 0)
     * and generate random RTP timestamp for this reading */
    if (wc_start_ == 0) {
        ts_        = uvg_rtp::random::generate_32();
        wc_start_2 = uvg_rtp::clock::hrc::now();
        wc_start_  = 1;
    }

    buffer[0] = 2 << 6; // RTP version
    buffer[1] = (payload_ & 0x7f) | (0 << 7);

    *(uint16_t *)&buffer[2] = htons(seq_);
    *(uint32_t *)&buffer[8] = htonl(ssrc_);

    if (timestamp_ == INVALID_TS) {
        *(uint32_t *)&buffer[4] = htonl(
            ts_
            + uvg_rtp::clock::hrc::diff_now(wc_start_2)
            * clock_rate_
            / 1000
        );
    } else {
        *(uint32_t *)&buffer[4] = htonl(timestamp_);
    }
}

void uvg_rtp::rtp::set_timestamp(uint64_t timestamp)
{
    timestamp_= timestamp;
}

uint32_t uvg_rtp::rtp::get_clock_rate(void)
{
    return clock_rate_;
}

void uvg_rtp::rtp::set_payload_size(size_t payload_size)
{
    payload_size_ = payload_size;
}

size_t uvg_rtp::rtp::get_payload_size()
{
    return payload_size_;
}

rtp_format_t uvg_rtp::rtp::get_payload()
{
    return (rtp_format_t)fmt_;
}
