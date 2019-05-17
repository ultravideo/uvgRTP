#ifdef _WIN32
// TODO
#else
#include <arpa/inet.h>
#endif

#include <stdint.h>
#include <cstring>
#include <iostream>

#include "debug.hh"
#include "conn.hh"
#include "rtp_generic.hh"
#include "send.hh"
#include "util.hh"
#include "writer.hh"

static int __internalWrite(RTPConnection *conn, uint8_t *buf, size_t bufLen, int flags)
{
    if (!buf || bufLen == 0)
        return RTP_INVALID_VALUE;

    RTPWriter *writer   = dynamic_cast<RTPWriter *>(conn);
    sockaddr_in outAddr = writer->getOutAddress();

#if __linux
    if (sendto(conn->getSocket(), buf, bufLen, flags, (struct sockaddr *)&outAddr, sizeof(outAddr)) == -1)
        return RTP_SEND_ERROR;
#else
    if ((WSASend(conn->getSocket(), buf, 1, bufLen, flags, NULL, NULL)) == SOCKET_ERROR)
        return RTP_SEND_ERROR;
#endif

    return RTP_OK;
}

int RTPSender::writePayload(RTPConnection *conn, uint8_t *payload, size_t payloadLen)
{
    if (!conn)
        return RTP_INVALID_VALUE;

#ifdef __RTP_STATS__
    conn->incProcessedBytes(payloadLen);
    conn->incTotalBytes(payloadLen);
    conn->incProcessedPackets(1);
#endif

    return __internalWrite(conn, payload, payloadLen, 0);
}

int RTPSender::writeGenericHeader(RTPConnection *conn, uint8_t *header, size_t headerLen)
{
    if (!conn)
        return RTP_INVALID_VALUE;

#ifdef __RTP_STATS__
    conn->incOverheadBytes(headerLen);
    conn->incTotalBytes(headerLen);
#endif

#ifdef __linux
    return __internalWrite(conn, header, headerLen, MSG_MORE);
#else
    return __internalWrite(conn, header, headerLen, MSG_PARTIAL);
#endif
}

int RTPSender::writeRTPHeader(RTPConnection *conn)
{
    if (!conn)
        return RTP_INVALID_VALUE;

    uint8_t header[RTP_HEADER_SIZE] = { 0 };

    header[0] = 2 << 6; // RTP version
    header[1] = (conn->getPayloadType() & 0x7f) | (0 << 7);

    *(uint16_t *)&header[2] = htons(conn->getSequence());
    *(uint32_t *)&header[4] = htonl(conn->getTimestamp());
    *(uint32_t *)&header[8] = htonl(conn->getSSRC());

    return RTPSender::writeGenericHeader(conn, header, RTP_HEADER_SIZE);
}
