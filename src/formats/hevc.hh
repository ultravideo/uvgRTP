#pragma once

#include "formats/generic.hh"

namespace kvz_rtp {
    class connection;
    class reader;

    namespace hevc {

        typedef struct media_headers {
            uint8_t nal_header[kvz_rtp::frame::HEADER_SIZE_HEVC_NAL];

            /* there are three types of Fragmentation Unit headers:
             *  - header for the first fragment
             *  - header for all middle fragments
             *  - header for the last fragment */
            uint8_t fu_headers[3 * kvz_rtp::frame::HEADER_SIZE_HEVC_FU];
        } media_headers_t;

        /* TODO:  */
        rtp_error_t push_frame(kvz_rtp::connection *conn, uint8_t *data, size_t data_len, int flags);

        /* TODO:  */
        rtp_error_t push_frame(kvz_rtp::connection *conn, std::unique_ptr<uint8_t[]> data, size_t data_len, int flags);

        /* TODO:  */
        rtp_error_t frame_receiver(kvz_rtp::reader *reader);
    };
};
