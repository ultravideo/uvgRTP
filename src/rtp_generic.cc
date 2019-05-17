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
#include "util.hh"
#include "writer.hh"

RTPGeneric::GenericFrame *RTPGeneric::createGenericFrame()
{
    RTPGeneric::GenericFrame *frame = new RTPGeneric::GenericFrame();

    if (!frame)
        return nullptr;

    frame->data          = nullptr;
    frame->dataLen       = 0;

    frame->rtp_timestamp = 0;
    frame->marker        = 0;
    frame->rtp_ssrc      = 0;
    frame->rtp_sequence  = 0;
    frame->rtp_payload   = 0;

    frame->rtp_format    = RTP_FORMAT_GENERIC;

    return frame;
}

RTPGeneric::GenericFrame *RTPGeneric::createGenericFrame(uint32_t dataLen)
{
    RTPGeneric::GenericFrame *frame = RTPGeneric::createGenericFrame();

    if (!frame)
        return nullptr;

    if ((frame->data = new uint8_t[dataLen + RTP_HEADER_SIZE]) == nullptr) {
        delete frame;
        return nullptr;
    }

    frame->dataLen = dataLen + RTP_HEADER_SIZE;
    frame->data   += RTP_HEADER_SIZE;

    return frame;
}

void RTPGeneric::destroyGenericFrame(RTPGeneric::GenericFrame *frame)
{
    if (!frame)
        return;

    if (frame->data)
        delete (frame->data - RTP_HEADER_SIZE);

    delete frame;
}

int RTPGeneric::pushGenericFrame(RTPConnection *conn, RTPGeneric::GenericFrame *frame)
{
    if (!conn || !frame)
        return RTP_INVALID_VALUE;

    uint8_t *buffer = frame->data - RTP_HEADER_SIZE;

    buffer[0] = 2 << 6; /* RTP version */
    buffer[1] = (frame->rtp_format & 0x7f) | (0 << 7);

    *(uint16_t *)&buffer[2] = htons(frame->rtp_sequence);
    *(uint32_t *)&buffer[4] = htonl(frame->rtp_timestamp);
    *(uint32_t *)&buffer[8] = htonl(frame->rtp_ssrc);

    RTPWriter *writer   = dynamic_cast<RTPWriter *>(conn);
    sockaddr_in outAddr = writer->getOutAddress();

    if (sendto(conn->getSocket(), buffer, frame->dataLen, 0, (struct sockaddr *)&outAddr, sizeof(outAddr)) == -1) {
        perror("pushGenericFrame");
        return RTP_GENERIC_ERROR;
    }

    conn->incRTPSequence(1);
    frame->rtp_sequence++;

    /* Update statistics */
#ifdef __RTP_STATS__
    conn->incProcessedBytes(frame->dataLen - RTP_HEADER_SIZE);
    conn->incOverheadBytes(RTP_HEADER_SIZE);
    conn->incTotalBytes(frame->dataLen);
    conn->incProcessedPackets(1);
#endif

    return RTP_OK;
}

int RTPGeneric::pushGenericFrame(RTPConnection *conn, uint8_t *data, uint32_t dataLen, uint32_t timestamp)
{
    (void)timestamp;

    if (!conn || !data)
        return RTP_INVALID_VALUE;

    uint8_t buffer[MAX_PACKET] = { 0 };

    buffer[0] = 2 << 6; // RTP version
    buffer[1] = (conn->getPayloadType() & 0x7f) | (0 << 7);

    *(uint16_t *)&buffer[2] = htons(conn->getSequence());
    *(uint32_t *)&buffer[4] = htonl(conn->getTimestamp());
    *(uint32_t *)&buffer[8] = htonl(conn->getSSRC());

    memcpy(&buffer[12], data, dataLen);

    RTPWriter *writer   = dynamic_cast<RTPWriter *>(conn);
    sockaddr_in outAddr = writer->getOutAddress();

    if (sendto(conn->getSocket(), buffer, dataLen + 12, 0, (struct sockaddr *)&outAddr, sizeof(outAddr)) == -1) {
        perror("pushGenericFrame");
        return RTP_GENERIC_ERROR;
    }

    conn->incRTPSequence(1);

#ifdef __RTP_STATS__
    /* Update statistics */
    conn->incProcessedBytes(dataLen);
    conn->incOverheadBytes(12);
    conn->incTotalBytes(dataLen + 12);
    conn->incProcessedPackets(1);
#endif

    return RTP_OK;
}
