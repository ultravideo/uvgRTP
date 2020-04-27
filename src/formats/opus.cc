#include <cstring>
#include <iostream>

#include "debug.hh"
#include "send.hh"

#include "formats/opus.hh"

/* Validity of arguments is checked by uvg_rtp::sender. We just need to relay them there */
rtp_error_t uvg_rtp::opus::push_frame(uvg_rtp::sender *sender, uint8_t *data, uint32_t data_len, int flags)
{
    return uvg_rtp::generic::push_frame(sender, data, data_len, flags);

#if 0
    rtp_error_t ret;

    if ((ret = uvg_rtp::sender::write_rtp_header(conn, timestamp)) != RTP_OK) {
        LOG_ERROR("Failed to write RTP Header for Opus frame!");
        return ret;
    }

    return uvg_rtp::sender::write_payload(conn, data, data_len);

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

rtp_error_t uvg_rtp::opus::push_frame(uvg_rtp::sender *sender, std::unique_ptr<uint8_t[]> data, uint32_t data_len, int flags)
{
    return uvg_rtp::generic::push_frame(sender, std::move(data), data_len, flags);
}
