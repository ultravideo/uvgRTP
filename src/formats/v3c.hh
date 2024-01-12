#pragma once

#include "uvgrtp/util.hh"
#include "uvgrtp/clock.hh"
#include "uvgrtp/frame.hh"

#include "h26x.hh"
#include "socket.hh"

#include <deque>
#include <memory>

namespace uvgrtp {

    class rtp;

    namespace formats {

        constexpr uint8_t HEADER_SIZE_V3C_PAYLOAD = 2;
        constexpr uint8_t HEADER_SIZE_V3C_NAL = 2;
        constexpr uint8_t HEADER_SIZE_V3C_FU = 1;

        enum V3C_NAL_TYPES {
            NAL_BLA_W_LP,
            NAL_RSV_IRAP_ACL_29
        };

        enum V3C_PKT_TYPES {
            V3C_PKT_AGGR = 56,
            V3C_PKT_FRAG = 57
        };

        struct v3c_aggregation_packet {
            uint8_t payload_header[HEADER_SIZE_V3C_PAYLOAD];
            uvgrtp::buf_vec nalus;  /* discrete NAL units */
            uvgrtp::buf_vec aggr_pkt; /* crafted aggregation packet */
        };

        struct v3c_headers {
            uint8_t payload_header[HEADER_SIZE_V3C_PAYLOAD];

            /* there are three types of Fragmentation Unit headers:
             *  - header for the first fragment
             *  - header for all middle fragments
             *  - header for the last fragment */
            uint8_t fu_headers[3 * HEADER_SIZE_V3C_FU];
        };

        class v3c : public h26x {
        public:
            v3c(std::shared_ptr<uvgrtp::socket> socket, std::shared_ptr<uvgrtp::rtp> rtp, int rce_flags);
            ~v3c();

        protected:

            // constructs v3c RTP header with correct values
            virtual rtp_error_t fu_division(uint8_t* data, size_t data_len, size_t payload_size);

            virtual uint8_t get_nal_type(uint8_t* data) const;

            virtual void get_nal_header_from_fu_headers(size_t fptr, uint8_t* frame_payload, uint8_t* complete_payload);

            virtual uint8_t get_payload_header_size() const;
            virtual uint8_t get_nal_header_size() const;
            virtual uint8_t get_fu_header_size() const;
            virtual uint8_t get_start_code_range() const;
            virtual uvgrtp::formats::FRAG_TYPE get_fragment_type(uvgrtp::frame::rtp_frame* frame) const;
            virtual uvgrtp::formats::NAL_TYPE  get_nal_type(uvgrtp::frame::rtp_frame* frame) const;
        };
    }
}

namespace uvg_rtp = uvgrtp;
