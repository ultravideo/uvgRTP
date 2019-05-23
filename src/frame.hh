#pragma once

#include "util.hh"

#define INVALID_FRAME_TYPE(ft) (ft < FRAME_TYPE_GENERIC || ft > FRAME_TYPE_HEVC_FU)

namespace kvz_rtp {
    namespace frame {
        enum HEADER_SIZES {
            HEADER_SIZE_RTP      = 12,
            HEADER_SIZE_OPUS     =  1,
            HEADER_SIZE_HEVC_RTP =  2,
            HEADER_SIZE_HEVC_FU  =  1,
        };

        typedef enum FRAME_TYPE {
            FRAME_TYPE_GENERIC = 0, // payload length + RTP Header size (N + 12)
            FRAME_TYPE_OPUS    = 1, // payload length + RTP Header size + Opus header (N + 12 + 0 [for now])
            FRAME_TYPE_HEVC_FU = 2, // payload length + RTP Header size + HEVC RTP Header + FU Header (N + 12 + 2 + 1)
        } frame_type_t;

        struct rtp_frame {
            uint32_t timestamp;
            uint32_t ssrc;
            uint16_t seq;
            uint8_t  ptype;
            uint8_t  marker;

            size_t total_len;   /* total length of the frame (payload length + header length) */
            size_t header_len;  /* length of header (varies based on the type of the frame) */
            size_t payload_len; /* length of the payload  */

            uint8_t *data;     /* pointer to the start of the whole buffer */
            uint8_t *payload;  /* pointer to actual payload */

            rtp_format_t format;
            frame_type_t type;
        };

        /* TODO:  */
        rtp_frame *alloc_frame(size_t payload_len, frame_type_t type);

        /* TODO:  */
        rtp_error_t dealloc_frame(kvz_rtp::frame::rtp_frame *frame);

        /* get pointer to rtp header start or nullptr if frame is invalid */
        uint8_t *get_rtp_header(kvz_rtp::frame::rtp_frame *frame);

        /* get pointer to opus header start or nullptr if frame is invalid */
        uint8_t *get_opus_header(kvz_rtp::frame::rtp_frame *frame);

        /* get pointer to hevc rtp header start or nullptr if frame is invalid */
        uint8_t *get_hevc_rtp_header(kvz_rtp::frame::rtp_frame *frame);

        /* get pointer to hevc fu header start or nullptr if frame is invalid */
        uint8_t *get_hevc_fu_header(kvz_rtp::frame::rtp_frame *frame);
    };
};
