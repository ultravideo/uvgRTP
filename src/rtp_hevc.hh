#pragma once

#include "rtp_generic.hh"

namespace kvz_rtp {
    class connection;
    class reader;

    namespace hevc {

        enum FRAG_TYPES {
            FT_INVALID   = -2, /* invalid combination of S and E bits */
            FT_NOT_FRAG  = -1, /* frame doesn't contain HEVC fragment */
            FT_START     =  1, /* frame contains a fragment with S bit set */
            FT_MIDDLE    =  2, /* frame is fragment but not S or E fragment */
            FT_END       =  3, /* frame contains a fragment with E bit set */
        };

        rtp_error_t push_frame(kvz_rtp::connection *conn, uint8_t *data, size_t data_len, uint32_t timestamp);

        /* Inspect the type of "frame" and return its type to caller
         *
         * Return FT_START when a frame with an S-bit set is received
         * Return FT_MIDDLE when a frame with neither S nor E-bit set is received (aka middle frag)
         * Return FT_END when a frame with E-bit set is received
         * Return FT_INVALID if the fragment contains invalid data
         * Return FT_NOT_FRAG if the frame doesn't contain HEVC fragment
         *
         * Return RTP_OK when E bit of the NAL header is set */
        int check_frame(kvz_rtp::frame::rtp_frame *frame);

        /* TODO:  */
        /* rtp_error_t frame_receiver(kvz_rtp::reader *reader); */

        rtp_error_t frame_receiver(kvz_rtp::reader *reader);
    };
};
