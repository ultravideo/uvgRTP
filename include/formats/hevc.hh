#pragma once

#include "formats/generic.hh"

namespace uvg_rtp {
    class sender;
    class reader;

    namespace hevc {

        typedef struct media_headers {
            uint8_t nal_header[uvg_rtp::frame::HEADER_SIZE_HEVC_NAL];

            /* there are three types of Fragmentation Unit headers:
             *  - header for the first fragment
             *  - header for all middle fragments
             *  - header for the last fragment */
            uint8_t fu_headers[3 * uvg_rtp::frame::HEADER_SIZE_HEVC_FU];
        } media_headers_t;

        /* TODO:  */
        rtp_error_t push_frame(uvg_rtp::sender *sender, uint8_t *data, size_t data_len, int flags);

        /* TODO:  */
        rtp_error_t push_frame(uvg_rtp::sender *sender, std::unique_ptr<uint8_t[]> data, size_t data_len, int flags);

        /* TODO:  */
        rtp_error_t frame_receiver(uvg_rtp::receiver *receiver, bool optimistic);
    };
};
