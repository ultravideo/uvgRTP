#pragma once

#include <string>

#include "util.hh"

#define INVALID_FRAME_TYPE(ft) (ft < FRAME_TYPE_GENERIC || ft > FRAME_TYPE_HEVC_FU)

namespace kvz_rtp {
    namespace frame {
        enum HEADER_SIZES {
            HEADER_SIZE_RTP      = 12,
            HEADER_SIZE_OPUS     =  1,
            HEADER_SIZE_HEVC_NAL =  2,
            HEADER_SIZE_HEVC_FU  =  1,
        };

        typedef enum RTP_FRAME_TYPE {
            FRAME_TYPE_GENERIC = 0, /* payload length + RTP Header size (N + 12) */
            FRAME_TYPE_OPUS    = 1, /* payload length + RTP Header size + Opus header (N + 12 + 0 [for now]) */
            FRAME_TYPE_HEVC_FU = 2, /* payload length + RTP Header size + HEVC NAL Header + FU Header (N + 12 + 2 + 1) */
        } rtp_type_t;

        typedef enum RTCP_FRAME_TYPE {
            FRAME_TYPE_SR   = 200, /* Sender report */
            FRAME_TYPE_RR   = 201, /* Receiver report */
            FRAME_TYPE_SDES = 202, /* Source description */
            FRAME_TYPE_BYE  = 203, /* Goodbye */
            FRAME_TYPE_APP  = 204  /* Application-specific message */
        } rtcp_type_t;

        PACKED_STRUCT(rtp_header) {
            uint8_t version:2;
            uint8_t padding:1;
            uint8_t ext:1;
            uint8_t cc:4;
            uint8_t marker:1;
            uint8_t payload:7;
            uint16_t seq;
            uint32_t timestamp;
            uint32_t ssrc;
        };

        PACKED_STRUCT(ext_header) {
            uint16_t type;
            uint16_t len;
            uint8_t *data;
        };

        struct rtp_frame {
            struct rtp_header header;
            uint32_t *csrc;
            struct ext_header *ext;

            size_t padding_len; /* non-zero if frame is padded */
            size_t payload_len; /* payload_len: total_len - header_len - padding length (if padded) */

            uint8_t *payload;

            rtp_format_t format;
            rtp_type_t type;
        };

        PACKED_STRUCT(rtcp_header) {
            uint8_t version:2;
            uint8_t padding:1;
            uint8_t count:5;
            uint8_t pkt_type;
            uint16_t length;
        };

        PACKED_STRUCT(rtcp_sender_info) {
            uint32_t ntp_msw; /* NTP timestamp, most significant word */
            uint32_t ntp_lsw; /* NTP timestamp, least significant word */
            uint32_t rtp_ts;  /* RTP timestamp corresponding to same time as NTP */
            uint32_t pkt_cnt;
            uint32_t byte_cnt;
        };

        PACKED_STRUCT(rtcp_report_block) {
            uint32_t ssrc;
            uint8_t  fraction;
            int32_t  lost:24;
            uint32_t last_seq;
            uint32_t jitter;
            uint32_t lsr;  /* last Sender Report */
            uint32_t dlsr; /* delay since last Sender Report */
        };

        PACKED_STRUCT(rtcp_sender_frame) {
            struct rtcp_header header;
            uint32_t sender_ssrc;
            struct rtcp_sender_info s_info;
            struct rtcp_report_block blocks[0];
        };

        PACKED_STRUCT(rtcp_receiver_frame) {
            struct rtcp_header header;
            uint32_t sender_ssrc;
            struct rtcp_report_block blocks[0];
        };

        PACKED_STRUCT(rtcp_sdes_item) {
            uint8_t type;
            uint8_t length;
            uint8_t data[0];
        };

        PACKED_STRUCT(rtcp_sdes_frame) {
            struct rtcp_header header;
            uint32_t sender_ssrc;
            struct rtcp_sdes_item items[0];
        };

        PACKED_STRUCT(rtcp_bye_frame) {
            struct rtcp_header header;
            uint32_t ssrc[0];
        };

        PACKED_STRUCT(rtcp_app_frame) {
            uint8_t version:2;
            uint8_t padding:1;
            uint8_t pkt_subtype:5;
            uint8_t pkt_type;
            uint16_t length;

            uint32_t ssrc;
            uint8_t name[4];
            uint8_t payload[0];
        };

        rtp_frame           *alloc_rtp_frame();
        rtp_frame           *alloc_rtp_frame(size_t payload_len);
        rtcp_app_frame      *alloc_rtcp_app_frame(std::string name, uint8_t subtype, size_t payload_len);
        rtcp_sdes_frame     *alloc_rtcp_sdes_frame(size_t ssrc_count, size_t total_len);
        rtcp_receiver_frame *alloc_rtcp_receiver_frame(size_t nblocks);
        rtcp_sender_frame   *alloc_rtcp_sender_frame(size_t nblocks);
        rtcp_bye_frame      *alloc_rtcp_bye_frame(size_t ssrc_count);

        rtp_error_t dealloc_frame(kvz_rtp::frame::rtp_frame *frame);

        /* TODO: template??? */
        rtp_error_t dealloc_frame(rtcp_sender_frame *frame);
        rtp_error_t dealloc_frame(rtcp_receiver_frame *frame);
        rtp_error_t dealloc_frame(rtcp_sdes_frame *frame);
        rtp_error_t dealloc_frame(rtcp_bye_frame *frame);
        rtp_error_t dealloc_frame(rtcp_app_frame *frame);

        /* get pointer to rtp header start or nullptr if frame is invalid */
        uint8_t *get_rtp_header(rtp_frame *frame);

        /* get pointer to opus header start or nullptr if frame is invalid */
        uint8_t *get_opus_header(rtp_frame *frame);

        /* get pointer to hevc rtp header start or nullptr if frame is invalid */
        uint8_t *get_hevc_nal_header(rtp_frame *frame);

        /* get pointer to hevc fu header start or nullptr if frame is invalid */
        uint8_t *get_hevc_fu_header(rtp_frame *frame);
    };
};
