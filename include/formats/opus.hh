#pragma once

#include <memory>

#include "formats/generic.hh"

namespace kvz_rtp {

    namespace opus {
        struct opus_config {
            uint32_t samplerate;
            uint8_t channels;
            uint8_t config_number;
        };

        /* TODO:  */
        rtp_error_t push_frame(kvz_rtp::sender *sender, uint8_t *data, uint32_t data_len, int flags);

        /* TODO:  */
        rtp_error_t push_frame(kvz_rtp::sender *sender, std::unique_ptr<uint8_t[]> data, uint32_t data_len, int flags);
    };
};
