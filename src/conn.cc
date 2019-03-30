#ifdef __linux__
#include <arpa/inet.h>
#endif
#include <cstring>
#include <iostream>

#include "conn.hh"
#include "util.hh"
#include "rtp_hevc.hh"
#include "rtp_opus.hh"

RTPConnection::RTPConnection(std::string dstAddr, int dstPort, int srcPort):
    dstAddr_(dstAddr),
    dstPort_(dstPort),
    srcPort_(srcPort),
    rtp_payload_(96)
{

    rtp_sequence_  = 45175;
    rtp_timestamp_ = 123456;
    rtp_ssrc_      = 0x72b644;
}

RTPConnection::~RTPConnection()
{
    delete inPacketBuffer_;
    delete runner_;
}

int RTPConnection::open()
{
    if ((socket_ = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("RTPConnection::open");
        return RTP_SOCKET_ERROR;
    }

    int value = 1;
    if (setsockopt(socket_, SOL_SOCKET, SO_REUSEADDR, &value, sizeof(int)) < 0)
        perror("setsockopt(SO_REUSEADDR) failed");

    memset(&addrOut_, 0, sizeof(addrOut_));
    addrOut_.sin_family = AF_INET;
    inet_pton(AF_INET, dstAddr_.c_str(), &addrOut_.sin_addr);
    addrOut_.sin_port = htons(dstPort_);

    memset(&addrIn_, 0, sizeof(addrIn_));
    addrIn_.sin_family = AF_INET;  
    addrIn_.sin_addr.s_addr = htonl(INADDR_ANY);
    addrIn_.sin_port = htons(srcPort_);

    /* if (bind(socket_, (struct sockaddr *) &addrIn_, sizeof(addrIn_)) < 0) { */
    /*     perror("RTPConnection::open"); */
    /*     return RTP_BIND_ERROR; */
    /* } */

    inPacketBuffer_ = new uint8_t[MAX_PACKET];
    inPacketBufferLen_ = MAX_PACKET;

    /* Start a blocking recv thread */
    runner_ = new std::thread(rtpRecvData, this);

    id_ = rtpGetUniqueId();

    return 0;
}

int RTPConnection::pushFrame(uint8_t *data, uint32_t datalen, rtp_format_t fmt, uint32_t timestamp)
{
    switch (fmt) {
        case RTP_FORMAT_HEVC:
            return RTPHevc::pushHevcFrame(this, data, datalen, timestamp);
        case RTP_FORMAT_OPUS:
            return RTPOpus::pushOpusFrame(this, data, datalen, timestamp);
        default:
            return RTPGeneric::pushGenericFrame(this, data, datalen, timestamp);
    }
}

RTPGeneric::GenericFrame *RTPConnection::pullFrame()
{
    while (framesOut_.empty()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    framesMtx_.lock();
    auto nextFrame = framesOut_.front();
    framesOut_.erase(framesOut_.begin());
    framesMtx_.unlock();

    return nextFrame;
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

uint8_t RTPConnection::getPayload() const
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

sockaddr_in RTPConnection::getOutAddress() const
{
    return addrOut_;
}

uint8_t *RTPConnection::getInPacketBuffer() const
{
    return inPacketBuffer_;
}

uint32_t RTPConnection::getInPacketBufferLength() const
{
    return inPacketBufferLen_;
}

void RTPConnection::fillFrame(RTPGeneric::GenericFrame *frame)
{
    if (!frame)
        return;

    frame->rtp_sequence  = rtp_sequence_;
    frame->rtp_timestamp = rtp_timestamp_;
    frame->rtp_ssrc      = rtp_ssrc_;
    /* std::cerr << rtp_sequence_ << " " << rtp_timestamp_ << " " << rtp_ssrc_; */
}

void RTPConnection::addOutgoingFrame(RTPGeneric::GenericFrame *frame)
{
    if (!frame)
        return;

    framesOut_.push_back(frame);
}
