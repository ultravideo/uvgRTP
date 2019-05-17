#ifdef _WIN32

#else
#include <arpa/inet.h>
#endif

#include <stdint.h>
#include <cstring>
#include <iostream>

#include "rtp_generic.hh"
#include "conn.hh"
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

    if ((frame->data = new uint8_t[dataLen]) == nullptr) {
        delete frame;
        return nullptr;
    }

    frame->dataLen = dataLen;
    return frame;
}

void RTPGeneric::destroyGenericFrame(RTPGeneric::GenericFrame *frame)
{
    if (!frame)
        return;

    if (frame->data)
        delete frame->data;

    delete frame;
}

int RTPGeneric::pushGenericFrame(RTPConnection *conn, RTPGeneric::GenericFrame *frame)
{
    if (!conn || !frame)
        return -EINVAL;

    uint8_t buffer[MAX_PACKET] = { 0 };

    buffer[0] = 2 << 6; /* RTP version */
    buffer[1] = (frame->rtp_format & 0x7f) | (0 << 7);

    *(uint16_t *)&buffer[2] = htons(frame->rtp_sequence);
    *(uint32_t *)&buffer[4] = htonl(frame->rtp_timestamp);
    *(uint32_t *)&buffer[8] = htonl(frame->rtp_ssrc);

    if (frame->data)
        memcpy(&buffer[12], frame->data, frame->dataLen);

    RTPWriter *writer   = dynamic_cast<RTPWriter *>(conn);
    sockaddr_in outAddr = writer->getOutAddress();

    if (sendto(conn->getSocket(), buffer, frame->dataLen + 12, 0, (struct sockaddr *)&outAddr, sizeof(outAddr)) == -1) {
        perror("pushGenericFrame");
        return -errno;
    }

    conn->incRTPSequence(1);
    frame->rtp_sequence++;

    /* Update statistics */
    conn->incProcessedBytes(frame->dataLen);
    conn->incOverheadBytes(12);
    conn->incTotalBytes(frame->dataLen + 12);
    conn->incProcessedPackets(1);

    return 0;
}

int RTPGeneric::pushGenericFrame(RTPConnection *conn, uint8_t *data, uint32_t dataLen, uint32_t timestamp)
{
    (void)timestamp;

    if (!conn || !data)
        return -EINVAL;

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
        return -errno;
    }

    conn->incRTPSequence(1);

    /* Update statistics */
    conn->incProcessedBytes(dataLen);
    conn->incOverheadBytes(12);
    conn->incTotalBytes(dataLen + 12);
    conn->incProcessedPackets(1);

    return 0;
}
