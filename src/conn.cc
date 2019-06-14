#ifdef __linux__
#include <arpa/inet.h>
#include <unistd.h>
#endif

#include <cstring>
#include <iostream>

#include "conn.hh"
#include "debug.hh"
#include "rtp_hevc.hh"
#include "rtp_opus.hh"
#include "util.hh"

kvz_rtp::connection::connection(bool reader):
    config_(nullptr),
    socket_(),
    reader_(reader)
{
    rtp_sequence_  = 45175;
    rtp_ssrc_      = 0x72b644;
    rtp_payload_   = RTP_FORMAT_HEVC;
}

kvz_rtp::connection::~connection()
{
}

void kvz_rtp::connection::set_payload(rtp_format_t fmt)
{
    rtp_payload_ = fmt;
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

kvz_rtp::socket_t kvz_rtp::connection::get_raw_socket() const
{
    return socket_.get_raw_socket();
}

kvz_rtp::socket kvz_rtp::connection::get_socket() const
{
    return socket_;
}

void kvz_rtp::connection::inc_rtp_sequence(size_t n)
{
    rtp_sequence_ += n;
}

void kvz_rtp::connection::inc_processed_bytes(size_t n)
{
    processed_bytes_ += n;
}

void kvz_rtp::connection::inc_overhead_bytes(size_t n)
{
    overhead_bytes_ += n;
}

void kvz_rtp::connection::inc_total_bytes(size_t n)
{
    total_bytes_ += n;
}

void kvz_rtp::connection::inc_processed_pkts(size_t n)
{
    processed_pkts_ += n;
}

void kvz_rtp::connection::inc_processed_pkts()
{
    processed_pkts_++;
}

void kvz_rtp::connection::inc_rtp_sequence()
{
    rtp_sequence_++;
}

void kvz_rtp::connection::fill_rtp_header(uint8_t *buffer, uint32_t timestamp)
{
    if (!buffer)
        return;

    buffer[0] = 2 << 6; // RTP version
    buffer[1] = (rtp_payload_ & 0x7f) | (0 << 7);

    *(uint16_t *)&buffer[2] = htons(rtp_sequence_);
    *(uint32_t *)&buffer[4] = htonl(timestamp);
    *(uint32_t *)&buffer[8] = htonl(rtp_ssrc_);
}
