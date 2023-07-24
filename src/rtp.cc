#include "rtp.hh"

#include "uvgrtp/frame.hh"

#include "debug.hh"
#include "random.hh"
#include "memory.hh"

#include "global.hh"

#ifndef _WIN32
#include <arpa/inet.h>
#include <unistd.h>
#endif

#include <chrono>
#include <iostream>

#define INVALID_TS UINT64_MAX

uvgrtp::rtp::rtp(rtp_format_t fmt, std::shared_ptr<std::atomic<std::uint32_t>> ssrc, bool ipv6):
    ssrc_(ssrc),
    ts_(uvgrtp::random::generate_32()),
    seq_(uvgrtp::random::generate_32() & 0xffff),
    fmt_(fmt),
    payload_((uint8_t)fmt),
    clock_rate_(0),
    wc_start_(),
    wc_start_2(),
    sent_pkts_(0),
    timestamp_(INVALID_TS),
    sampling_ntp_(0),
    rtp_ts_(0),
    delay_(PKT_MAX_DELAY_MS)
{
    if (ipv6) {
        payload_size_ = MAX_IPV6_MEDIA_PAYLOAD;
    }
    else {
        payload_size_ = MAX_IPV4_MEDIA_PAYLOAD;
    }
    set_default_clock_rate(fmt);

}

uvgrtp::rtp::~rtp()
{
}

uint32_t uvgrtp::rtp::get_ssrc() const
{
    return *ssrc_.get();
}

uint16_t uvgrtp::rtp::get_sequence() const
{
    return seq_;
}

void uvgrtp::rtp::set_default_clock_rate(rtp_format_t fmt)
{
    switch (fmt) {
        case RTP_FORMAT_PCMU:
        case RTP_FORMAT_GSM:
        case RTP_FORMAT_G723:
        case RTP_FORMAT_DVI4_32:
        case RTP_FORMAT_PCMA:
        case RTP_FORMAT_LPC:
        case RTP_FORMAT_G722: // the RFC 1890 had an error, so now G722 is incorrect forever everywhere
        case RTP_FORMAT_G728:
        case RTP_FORMAT_G729:
        case RTP_FORMAT_G726_40:
        case RTP_FORMAT_G726_32:
        case RTP_FORMAT_G726_24:
        case RTP_FORMAT_G726_16:
        case RTP_FORMAT_G729D:
        case RTP_FORMAT_G729E:
        case RTP_FORMAT_GSM_EFR:
            clock_rate_ = 8000;
            break;
        case RTP_FORMAT_DVI4_441:
            clock_rate_ = 11025;
            break;
        case RTP_FORMAT_DVI4_64:
            clock_rate_ = 16000;
            break;
        case RTP_FORMAT_DVI4_882:
            clock_rate_ = 22050;
            break;
        case RTP_FORMAT_L16_STEREO:
        case RTP_FORMAT_L16_MONO:
            clock_rate_ = 44100;
            break;
        case RTP_FORMAT_OPUS:
            clock_rate_ = 48000;
            break;
        case RTP_FORMAT_H264:
        case RTP_FORMAT_H265:
        case RTP_FORMAT_H266:
            clock_rate_ = 90000;
            break;
        case RTP_FORMAT_L8:   // variable, user should set this
        case RTP_FORMAT_VDVI: // variable
            UVG_LOG_WARN("Using variable clock rate format, please set rate manually. Using 8000");
            clock_rate_ = 8000;
            break;
        default:
            UVG_LOG_WARN("Unknown RTP format, setting clock rate to 8000");
            clock_rate_ = 8000;
            break;
    }
}

void uvgrtp::rtp::set_dynamic_payload(uint8_t payload)
{
    payload_ = payload;
}
uint8_t uvgrtp::rtp::get_dynamic_payload() const
{
    return payload_;
}

void uvgrtp::rtp::inc_sequence()
{
    if (seq_ != UINT16_MAX) {
        seq_++;
    } else {
        seq_ = 0;
    }
}

void uvgrtp::rtp::inc_sent_pkts()
{
    sent_pkts_++;
}

void uvgrtp::rtp::update_sequence(uint8_t *buffer)
{
    if (!buffer)
        return;

    *(uint16_t *)&buffer[2] = htons(seq_);
}

void uvgrtp::rtp::fill_header(uint8_t *buffer)
{
    if (!buffer)
        return;

    /* This is the first RTP message, get wall clock reading (t = 0)
     * and generate random RTP timestamp for this reading */
    if (!ts_) {
        ts_        = uvgrtp::random::generate_32();
        wc_start_ = std::chrono::high_resolution_clock::now();
    }

    buffer[0] = 2 << 6; // RTP version
    buffer[1] = (payload_ & 0x7f) | (0 << 7);

    *(uint16_t *)&buffer[2] = htons(seq_);
    *(uint32_t *)&buffer[8] = htonl(*ssrc_.get());

    if (timestamp_ == INVALID_TS) {

        auto t1 = std::chrono::high_resolution_clock::now();
        std::chrono::microseconds time_since_start = 
            std::chrono::duration_cast<std::chrono::microseconds>(t1 - wc_start_);

        uint64_t u_seconds = time_since_start.count() * clock_rate_;

        uint32_t rtp_timestamp = ts_ + uint32_t(u_seconds / 1000000);
        rtp_ts_ = rtp_timestamp;
        sampling_ntp_ = uvgrtp::clock::ntp::now();

        *(uint32_t *)&buffer[4] = htonl((u_long)rtp_timestamp);

    }
    else {
        rtp_ts_ = (uint32_t)timestamp_;
        *(uint32_t *)&buffer[4] = htonl((u_long)timestamp_);
    }
}

void uvgrtp::rtp::set_timestamp(uint64_t timestamp)
{
    timestamp_= timestamp;
}

void uvgrtp::rtp::set_clock_rate(uint32_t rate)
{
    clock_rate_ = rate;
}

uint32_t uvgrtp::rtp::get_clock_rate() const
{
    return clock_rate_;
}

void uvgrtp::rtp::set_payload_size(size_t payload_size)
{
    payload_size_ = payload_size;
}

size_t uvgrtp::rtp::get_payload_size() const
{
    return payload_size_;
}

rtp_format_t uvgrtp::rtp::get_payload() const
{
    return (rtp_format_t)fmt_;
}

void uvgrtp::rtp::set_pkt_max_delay(size_t delay)
{
    delay_ = delay;
}

size_t uvgrtp::rtp::get_pkt_max_delay() const
{
    return delay_;
}

void uvgrtp::rtp::set_sampling_ntp(uint64_t ntp_ts) {
    sampling_ntp_ = ntp_ts;
}

uint64_t uvgrtp::rtp::get_sampling_ntp() const {
    return sampling_ntp_;
}

uint32_t uvgrtp::rtp::get_rtp_ts() const {
    return rtp_ts_;
}

rtp_error_t uvgrtp::rtp::packet_handler(void* args, int rce_flags, uint8_t* packet, size_t size, uvgrtp::frame::rtp_frame **out)
{
    (void)rce_flags;
    (void)args;

    /* not an RTP frame */
    if (size < 12)
    {
        UVG_LOG_DEBUG("Received RTP packet is too small to contain header");
        return RTP_PKT_NOT_HANDLED;
    }

    uint8_t *ptr = (uint8_t *)packet;

    /* invalid version */
    if (((ptr[0] >> 6) & 0x03) != 0x2)
    {
        UVG_LOG_DEBUG("Received RTP packet with invalid version");
        return RTP_PKT_NOT_HANDLED;
    }

    *out = uvgrtp::frame::alloc_rtp_frame();

    if (*out == nullptr)
    {
        return RTP_GENERIC_ERROR;
    }

    (*out)->header.version   = (ptr[0] >> 6) & 0x03;
    (*out)->header.padding   = (ptr[0] >> 5) & 0x01;
    (*out)->header.ext       = (ptr[0] >> 4) & 0x01;
    (*out)->header.cc        = (ptr[0] >> 0) & 0x0f;
    (*out)->header.marker    = (ptr[1] & 0x80) ? 1 : 0;
    (*out)->header.payload   = (ptr[1] & 0x7f);
    (*out)->header.seq       = ntohs(*(uint16_t *)&ptr[2]);
    (*out)->header.timestamp = ntohl(*(uint32_t *)&ptr[4]);
    (*out)->header.ssrc      = ntohl(*(uint32_t *)&ptr[8]);

    (*out)->payload_len = (size_t)size - sizeof(uvgrtp::frame::rtp_header);

    /* Skip the generic RTP header
     * There may be 0..N CSRC entries after the header, so check those */
    ptr += sizeof(uvgrtp::frame::rtp_header);

    if ((*out)->header.cc > 0) {
        UVG_LOG_DEBUG("frame contains csrc entries");

        if ((ssize_t)((*out)->payload_len - (*out)->header.cc * sizeof(uint32_t)) < 0) {
            UVG_LOG_DEBUG("Invalid frame length, %d CSRC entries, total length %zu", (*out)->header.cc, (*out)->payload_len);
            (void)uvgrtp::frame::dealloc_frame(*out);
            return RTP_GENERIC_ERROR;
        }
        UVG_LOG_DEBUG("Allocating %u CSRC entries", (*out)->header.cc);

        (*out)->csrc         = new uint32_t[(*out)->header.cc];
        (*out)->payload_len -= (*out)->header.cc * sizeof(uint32_t);

        for (size_t i = 0; i < (*out)->header.cc; ++i) {
            (*out)->csrc[i]  = *(uint32_t *)ptr;
            ptr             += sizeof(uint32_t);
        }
    }

    if ((*out)->header.ext) {
        UVG_LOG_DEBUG("Frame contains extension information");
        (*out)->ext = new uvgrtp::frame::ext_header;

        (*out)->ext->type    = ntohs(*(uint16_t *)&ptr[0]);
        (*out)->ext->len     = ntohs(*(uint16_t *)&ptr[2]) * sizeof(uint32_t);
        (*out)->ext->data    = (uint8_t *)memdup(ptr + 2 * sizeof(uint16_t), (*out)->ext->len);
        (*out)->payload_len -= 2 * sizeof(uint16_t) + (*out)->ext->len;
        ptr                 += 2 * sizeof(uint16_t) + (*out)->ext->len;
    }

    /* If padding is set to 1, the last byte of the payload indicates
     * how many padding bytes was used. Make sure the padding length is
     * valid and subtract the amount of padding bytes from payload length */
    if ((*out)->header.padding) {
        UVG_LOG_DEBUG("Frame contains padding");
        uint8_t padding_len = (*out)->payload[(*out)->payload_len - 1];

        if (!padding_len || (*out)->payload_len <= padding_len) {
            uvgrtp::frame::dealloc_frame(*out);
            return RTP_GENERIC_ERROR;
        }

        (*out)->payload_len -= padding_len;
        (*out)->padding_len  = padding_len;
    }

    (*out)->payload    = (uint8_t *)memdup(ptr, (*out)->payload_len);
    (*out)->dgram      = (uint8_t *)packet;
    (*out)->dgram_size = size;

    return RTP_PKT_MODIFIED;
}
