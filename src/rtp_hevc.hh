#pragma once

#include "rtp_generic.hh"

namespace kvz_rtp {
    class connection;
    class reader;

    namespace hevc {
        /* TODO:  */
        rtp_error_t push_frame(kvz_rtp::connection *conn, uint8_t *data, size_t data_len, int flags);

        /* TODO:  */
        rtp_error_t frame_receiver(kvz_rtp::reader *reader);
    };
};
