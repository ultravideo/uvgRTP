#include <cstring>
#include <iostream>

#include "conn.hh"
#include "debug.hh"
#include "rtp_opus.hh"
#include "send.hh"

/* Validity of arguments is checked by kvz_rtp::sender. We just need to relay them there */
rtp_error_t kvz_rtp::opus::push_frame(connection *conn, uint8_t *data, uint32_t data_len, uint32_t timestamp)
{
    return kvz_rtp::generic::push_frame(conn, data, data_len, timestamp);

#if 0
    rtp_error_t ret;

    if ((ret = kvz_rtp::sender::write_rtp_header(conn, timestamp)) != RTP_OK) {
        LOG_ERROR("Failed to write RTP Header for Opus frame!");
        return ret;
    }

    return kvz_rtp::sender::write_payload(conn, data, data_len);

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

kvz_rtp::frame::rtp_frame *kvz_rtp::opus::process_opus_frame(
    kvz_rtp::frame::rtp_frame *frame,
    std::pair<size_t, std::vector<kvz_rtp::frame::rtp_frame *>>& fu,
    rtp_error_t& error
)
{
    (void)fu;

    if (!frame) {
        error = RTP_INVALID_VALUE;
        
        LOG_ERROR("Invalid value, unable to process frame!");
        return nullptr;
    }

    error = RTP_OK;
    return frame;
}
