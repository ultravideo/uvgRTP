#pragma once

#include "rtp_generic.hh"

namespace RTPOpus {
    struct OpusConfig {
        uint32_t samplerate;
        uint8_t channels;
        uint8_t configurationNumber;
    };

    int pushOpusFrame(RTPConnection *conn, uint8_t *data, uint32_t dataLen, uint32_t timestamp);
};
