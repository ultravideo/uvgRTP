#pragma once

#include "rtp_generic.hh"

namespace kvz_rtp {
    class connection;

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

        /* Merge fragmentation units into one complete frames
         * "first" marks the index of the first fragmentation unit in "frames" array
         * "nframes" tells how many frames should be merged to together
         *
         * NOTE: merge_frames() will free all RTP frames from range "first - (first + nframes)" and sets
         * the range in the "frames" array to nullptr
         *
         * Return pointer to merged frame on succes
         * Return nullptr and set rtp_errno to RTP_MEMORY_ERROR if allocation fails
         * Return nullptr and set rtp_errno to RTP_INVALID_VALUE if one of the frames from range is missing */
        kvz_rtp::frame::rtp_frame *merge_frames(kvz_rtp::frame::rtp_frame **frames, uint16_t first, size_t nframes);

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
            std::pair<size_t, std::vector<kvz_rtp::frame::rtp_frame *>>& fu,
            rtp_error_t& error
        );
    };
};
