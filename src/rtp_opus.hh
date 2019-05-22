#pragma once

#include "rtp_generic.hh"

namespace kvz_rtp {
    namespace opus {
        struct opus_config {
            uint32_t samplerate;
            uint8_t channels;
            uint8_t config_number;
        };

        rtp_error_t push_opus_frame(connection *conn, uint8_t *data, uint32_t data_len, uint32_t timestamp);
    };
};
