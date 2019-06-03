#pragma once

#include "rtp_generic.hh"

namespace kvz_rtp {
    class connection;

    namespace hevc {
        rtp_error_t push_hevc_frame(kvz_rtp::connection *conn, uint8_t *data, size_t data_len, uint32_t timestamp);

        /* Process the incoming HEVC frame
         * The RTP frame "frame" given as parameter should be considered invalid after calling this function
         * and no operatios should be performed on it after the function has returned.
         *
         * On success, a valid RTP frame is returned and "error" is set to RTP_OK
         *
         * If the original frame has been split and this is a fragment of it, the fragment is returned
         * and "error" is set to RTP_NOT_READY
         *
         * If the frame is invalid, nullptr is returned and "error" is set to RTP_INVALID_VALUE (is possible) */
        kvz_rtp::frame::rtp_frame *process_hevc_frame(
            kvz_rtp::frame::rtp_frame *frame,
            std::vector<kvz_rtp::frame::rtp_frame *>& fu,
            rtp_error_t& error
        );
    };
};
