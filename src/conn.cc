#ifdef __linux__
#include <arpa/inet.h>
#include <unistd.h>
#endif
#include <cstring>
#include <iostream>

#include "conn.hh"
#include "rtp_hevc.hh"
#include "rtp_opus.hh"
#include "util.hh"

kvz_rtp::connection::connection(bool reader):
    config_(nullptr),
    reader_(reader)
{
    rtp_sequence_  = 45175;
    rtp_ssrc_      = 0x72b644;
    rtp_payload_   = RTP_FORMAT_HEVC;
}

kvz_rtp::connection::~connection()
{
#ifdef __linux__
    close(socket_);
#else
    /* TODO: close socket a la windows */
#endif
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

#ifdef _WIN32
SOCKET kvz_rtp::connection::get_socket() const
#else
int    kvz_rtp::connection::get_socket() const
#endif
{
    return socket_;
}

void kvz_rtp::connection::incRTPSequence(uint16_t seq)
{
    rtp_sequence_ += seq;
}

void kvz_rtp::connection::incProcessedBytes(uint32_t nbytes)
{
    processedBytes_ += nbytes;
}

void kvz_rtp::connection::incOverheadBytes(uint32_t nbytes)
{
    overheadBytes_ += nbytes;
}

void kvz_rtp::connection::incTotalBytes(uint32_t nbytes)
{
    totalBytes_ += nbytes;
}

void kvz_rtp::connection::incProcessedPackets(uint32_t npackets)
{
    processedPackets_ += npackets;
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
