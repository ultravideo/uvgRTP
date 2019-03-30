#pragma once

#include "rtputil.hh"

class RTPConnection;

namespace RTPGeneric {
    struct GenericFrame {
        uint32_t rtp_timestamp;
        uint32_t dataLen;
        uint32_t rtp_ssrc;
        uint16_t rtp_sequence;
        uint8_t *data;
        uint8_t rtp_payload;
        uint8_t marker;

        rtp_format_t rtp_format;
    };

    RTPGeneric::GenericFrame *createGenericFrame();
    RTPGeneric::GenericFrame *createGenericFrame(uint32_t dataLen);
    void destroyGenericFrame(RTPGeneric::GenericFrame *frame);

    int pushGenericFrame(RTPConnection *conn, RTPGeneric::GenericFrame *frame);
    int pushGenericFrame(RTPConnection *conn, uint8_t *data, uint32_t dataLen, uint32_t timestamp);
};
