#pragma once

#include "rtp_generic.hh"

namespace kvz_rtp {
    class connection;

    namespace hevc {
        rtp_error_t push_hevc_frame(kvz_rtp::connection *conn, uint8_t *data, size_t data_len, uint32_t timestamp);
    };
};
