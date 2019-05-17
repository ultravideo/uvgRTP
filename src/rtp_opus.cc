#include <cstring>
#include <iostream>

#include "conn.hh"
#include "debug.hh"
#include "rtp_opus.hh"
#include "rtp_generic.hh"

int RTPOpus::pushOpusFrame(RTPConnection *conn, uint8_t *data, uint32_t dataLen, uint32_t timestamp)
{
    if (!conn || !data)
        return RTP_INVALID_VALUE;

#if 0
    uint8_t buffer[MAX_PACKET]  = { 0 };
    RTPOpus::OpusConfig *config = (RTPOpus::OpusConfig *)conn->getConfig();

    LOG_DEBUG("sending opus packet of size %u", dataLen);

    if (config == nullptr) {
        LOG_ERROR("Opus config has not been set!");
        return RTP_INVALID_VALUE;
    }

    buffer[0] = (config->configurationNumber << 3) |
                (config->channels > 1 ? (1 << 2) : 0) | 0;

    memcpy(&buffer[1], data, dataLen);

    return RTPGeneric::pushGenericFrame(conn, buffer, dataLen + 1, 0);
#endif

    conn->setPayloadType(RTP_FORMAT_OPUS);
    return RTPGeneric::pushGenericFrame(conn, data, dataLen, 0);
}
