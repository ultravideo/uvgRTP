#pragma once

#include <vector>

#include "util.hh"

namespace uvg_rtp {
    class sender;

    namespace send {
        /* Send RTP Frame to remote
         *
         * This functions assumes "frame_len" is smaller than MAX_PAYLOAD
         * No measures are taken (apart from print a warning) if it's larger
         * TODO: should it split the frame?
         *
         * send_frame() assumes that "frame" starts with a valid RTP header
         *
         * Return RTP_OK on success
         * Return RTP_INVALID_VALUE if one of the values are invalid
         * Return RTP_SEND_ERROR if sending the frame failed */
        rtp_error_t send_frame(
            uvg_rtp::sender *sender,
            uint8_t *frame, size_t frame_len
        );

        /* Send RTP Frame to remote
         *
         * This functions assumes "frame_len" + "header_len" is smaller than MAX_PAYLOAD
         * No measures are taken (apart from print a warning) if it's larger
         * TODO: should it split the frame?
         *
         * send_frame() assumes that "header" points to a valid RTP header
         *
         * Return RTP_OK on success
         * Return RTP_INVALID_VALUE if one of the values are invalid
         * Return RTP_SEND_ERROR if sending the frame failed */
        rtp_error_t send_frame(
            uvg_rtp::sender *sender,
            uint8_t *header, size_t header_len,
            uint8_t *payload, size_t payload_len
        );

        /* Send RTP Frame to remote
         *
         * This functions assumes "frame_len" is smaller than MAX_PAYLOAD
         * No measures are taken (apart from print a warning) if it's larger
         * TODO: should it split the frame?
         *
         * send_frame() assumes that "buffers" contains at least two buffers:
         *  - RTP header
         *  - RTP payload
         *
         * RTP header must be the first buffer of the "buffers" vector
         *
         * Return RTP_OK on success
         * Return RTP_INVALID_VALUE if one of the values are invalid
         * Return RTP_SEND_ERROR if sending the frame failed */
        rtp_error_t send_frame(
            uvg_rtp::sender *sender,
            std::vector<std::pair<size_t, uint8_t *>>& buffers
        );

    };
};
