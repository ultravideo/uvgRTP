#ifdef __linux__
#include <arpa/inet.h>
#endif
#include <cstring>
#include <iostream>

#include "conn.hh"
#include "util.hh"
#include "rtp_hevc.hh"
#include "rtp_opus.hh"

RTPConnection::RTPConnection(bool reader):
    reader_(reader)
{
    rtp_sequence_  = 45175;
    rtp_timestamp_ = 123456;
    rtp_ssrc_      = 0x72b644;
    rtp_payload_   = RTP_FORMAT_HEVC;
}

RTPConnection::~RTPConnection()
{
}

void RTPConnection::setPayloadType(rtp_format_t fmt)
{
    rtp_payload_ = fmt;
}

void RTPConnection::setConfig(void *config)
{
    config_ = static_cast<void *>(config);
}

void *RTPConnection::getConfig()
{
    return config_;
}

uint32_t RTPConnection::getId() const
{
    return id_;
}

uint16_t RTPConnection::getSequence() const
{
    return rtp_sequence_;
}

uint32_t RTPConnection::getTimestamp() const
{
    return rtp_timestamp_;
}

uint32_t RTPConnection::getSSRC() const
{
    return rtp_ssrc_;
}

uint8_t RTPConnection::getPayloadType() const
{
    return rtp_payload_;
}

int RTPConnection::getSocket() const
{
    return socket_;
}

void RTPConnection::incRTPSequence(uint16_t seq)
{
    rtp_sequence_ += seq;
}

void RTPConnection::incProcessedBytes(uint32_t nbytes)
{
    processedBytes_ += nbytes;
}

void RTPConnection::incOverheadBytes(uint32_t nbytes)
{
    overheadBytes_ += nbytes;
}

void RTPConnection::incTotalBytes(uint32_t nbytes)
{
    totalBytes_ += nbytes;
}

void RTPConnection::incProcessedPackets(uint32_t npackets)
{
    processedPackets_ += npackets;
}

void RTPConnection::fillFrame(RTPGeneric::GenericFrame *frame)
{
    if (!frame)
        return;

    frame->rtp_sequence  = rtp_sequence_;
    frame->rtp_timestamp = rtp_timestamp_;
    frame->rtp_ssrc      = rtp_ssrc_;
}
