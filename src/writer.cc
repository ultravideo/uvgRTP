#include <arpa/inet.h>
#include <cstring>
#include <iostream>

#include "writer.hh"
#include "rtp_opus.hh"
#include "rtp_hevc.hh"
#include "rtp_generic.hh"

RTPWriter::RTPWriter(std::string dstAddr, int dstPort):
    RTPConnection(false),
    dstAddr_(dstAddr),
    dstPort_(dstPort)
{
}

RTPWriter::~RTPWriter()
{
}

int RTPWriter::start()
{
    if ((socket_ = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("RTPConnection::open");
        return RTP_SOCKET_ERROR;
    }

    memset(&addrOut_, 0, sizeof(addrOut_));
    addrOut_.sin_family = AF_INET;
    inet_pton(AF_INET, dstAddr_.c_str(), &addrOut_.sin_addr);
    addrOut_.sin_port = htons(dstPort_);

    id_ = rtpGetUniqueId();
    return 0;
}

int RTPWriter::pushFrame(uint8_t *data, uint32_t datalen, rtp_format_t fmt, uint32_t timestamp)
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

sockaddr_in RTPWriter::getOutAddress()
{
    return addrOut_;
}
