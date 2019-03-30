#include <cstring>
#include <iostream>

#include "conn.hh"
#include "rtp_opus.hh"
#include "rtp_generic.hh"

int RTPOpus::pushOpusFrame(RTPConnection *conn, uint8_t *data, uint32_t dataLen, uint32_t timestamp)
{
    uint8_t buffer[MAX_PACKET]  = { 0 };
    RTPOpus::OpusConfig *config = (RTPOpus::OpusConfig *)conn->getConfig();

    buffer[0] = (config->configurationNumber << 3) |
                (config->channels > 1 ? (1 << 2) : 0) | 0;

    memcpy(&buffer[1], data, dataLen);
    conn->setPayload(RTP_FORMAT_OPUS);

    return RTPGeneric::pushGenericFrame(conn, buffer, dataLen + 1, 0);
}
