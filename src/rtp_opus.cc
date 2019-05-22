#include <cstring>
#include <iostream>

#include "conn.hh"
#include "debug.hh"
#include "rtp_opus.hh"
#include "send.hh"

/* Validity of arguments is checked by kvz_rtp::sender. We just need to relay them there */
rtp_error_t kvz_rtp::opus::push_opus_frame(connection *conn, uint8_t *data, uint32_t data_len, uint32_t timestamp)
{
    rtp_error_t ret;

    if ((ret = kvz_rtp::sender::write_rtp_header(conn, timestamp)) != RTP_OK) {
        LOG_ERROR("Failed to write RTP Header for Opus frame!");
        return ret;
    }

    return kvz_rtp::sender::write_payload(conn, data, data_len);

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

    conn->setPayloadType(RTP_FORMAT_OPUS);
    return RTPGeneric::pushGenericFrame(conn, data, dataLen, 0);
#endif
}
