#include <arpa/inet.h>
#include <cstring>
#include <iostream>

#include "debug.hh"
#include "rtp_opus.hh"
#include "rtp_hevc.hh"
#include "rtp_generic.hh"
#include "writer.hh"

RTPWriter::RTPWriter(std::string dstAddr, int dstPort):
    RTPConnection(false),
    dstAddr_(dstAddr),
    dstPort_(dstPort),
    srcPort_(0)
{
}

RTPWriter::RTPWriter(std::string dstAddr, int dstPort, int srcPort):
    RTPWriter(dstAddr, dstPort)
{
    srcPort_ = srcPort;
}

RTPWriter::~RTPWriter()
{
}

int RTPWriter::start()
{
    if ((socket_ = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        LOG_ERROR("Creating socket failed: %s", strerror(errno));
        return RTP_SOCKET_ERROR;
    }

    /* if source port is not 0, writer should be bind to that port so that outgoing packet
     * has a correct source port (important for hole punching purposes) */
    if (srcPort_ != 0) {
        int enable = 1;
        if (setsockopt(socket_, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0) {
            perror("setsockopt(SO_REUSEADDR) failed");
            return RTP_GENERIC_ERROR;
        }

        LOG_DEBUG("Binding to port %d (source port)", srcPort_);

        sockaddr_in addrIn_;

        memset(&addrIn_, 0, sizeof(addrIn_));
        addrIn_.sin_family = AF_INET;
        addrIn_.sin_addr.s_addr = htonl(INADDR_ANY);
        addrIn_.sin_port = htons(srcPort_);

        if (bind(socket_, (struct sockaddr *) &addrIn_, sizeof(addrIn_)) < 0) {
            LOG_ERROR("Binding failed: %s", strerror(errno));
            return RTP_BIND_ERROR;
        }
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
