#ifdef _WIN32
#else
#include <arpa/inet.h>
#endif
#include <iostream>
#include <cstring>

#include "util.hh"
#include "conn.hh"
#include "rtp_generic.hh"

uint64_t rtpGetUniqueId()
{
    static uint64_t i = 1;
    return i++;
}

int rtpRecvData(RTPConnection *conn)
{
    sockaddr_in fromAddr;

    std::cerr << "starting to listen to address and port" << std::endl;

    while (1) {
        int fromAddrSize = sizeof(fromAddr);
        int32_t ret = recvfrom(conn->getSocket(), conn->getInPacketBuffer(), conn->getInPacketBufferLength(),
                               0, /* no flags */
#ifdef _WIN32
                               (SOCKADDR *)&fromAddr, 
                               &fromAddrSize
#else
                               (struct sockaddr *)&fromAddr,
                               (socklen_t *)&fromAddrSize
#endif
                );

        if (ret == -1) {
#if _WIN32
            int _error = WSAGetLastError();
            if (_error != 10035)
                std::cerr << "Socket error" << _error << std::endl;
#else
            perror("rtpRecvData:");
#endif
            return -1;
        } else {
            const uint8_t *inBuffer = conn->getInPacketBuffer();

            RTPGeneric::GenericFrame *frame = RTPGeneric::createGenericFrame();

            frame->marker      = (inBuffer[1] & 0x80) ? 1 : 0;
            frame->rtp_payload = (inBuffer[1] & 0x7f);

            frame->rtp_sequence  = ntohs(*(uint16_t *)&inBuffer[2]);
            frame->rtp_timestamp = ntohl(*(uint32_t *)&inBuffer[4]);
            frame->rtp_ssrc      = ntohl(*(uint32_t *)&inBuffer[8]);
            frame->data          = new uint8_t[ret - 12];
            frame->dataLen       = ret - 12;

            memcpy(frame->data, &inBuffer[12], frame->dataLen);

            conn->addOutgoingFrame(frame);
        }
    }
}
