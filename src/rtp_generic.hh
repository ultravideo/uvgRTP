#pragma once

#include "util.hh"

namespace kvz_rtp {
    class connection;

    namespace generic {
        rtp_error_t push_generic_frame(kvz_rtp::connection *conn, uint8_t *data, size_t data_len, uint32_t timestamp);
    };
};
