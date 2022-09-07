#pragma once

#include "h26x.hh"

#include "uvgrtp/clock.hh"
#include "uvgrtp/util.hh"
#include "uvgrtp/frame.hh"

#include "socket.hh"

#include <deque>
#include <map>
#include <unordered_set>
#include <vector>
#include <memory>

namespace uvgrtp {

    namespace formats {

        constexpr uint8_t HEADER_SIZE_H265_PAYLOAD = 2;
        constexpr uint8_t HEADER_SIZE_H265_NAL = 2;
        constexpr uint8_t HEADER_SIZE_H265_FU = 1;

        enum H265_NAL_TYPES {
            H265_TRAIL_R = 1,
            H265_IDR_W_RADL = 19,
            H265_PKT_AGGR = 48,
            H265_PKT_FRAG = 49
        };

        struct h265_aggregation_packet {
            uint8_t payload_header[HEADER_SIZE_H265_PAYLOAD] = {0};
            uvgrtp::buf_vec nalus;  /* discrete NAL units */
            uvgrtp::buf_vec aggr_pkt; /* crafted aggregation packet */
        };

        struct h265_headers {
            uint8_t payload_header[HEADER_SIZE_H265_PAYLOAD];

            /* there are three types of Fragmentation Unit headers:
             *  - header for the first fragment
             *  - header for all middle fragments
             *  - header for the last fragment */
            uint8_t fu_headers[3 * HEADER_SIZE_H265_FU];
        };

        class h265 : public h26x {
            public:
                h265(std::shared_ptr<uvgrtp::socket> socket, std::shared_ptr<uvgrtp::rtp> rtp, int rce_flags);
                ~h265();

            protected:
                /* Construct an aggregation packet from data in "aggr_pkt_info_" */
                virtual rtp_error_t finalize_aggregation_pkt();

                /* Clear aggregation buffers */
                virtual void clear_aggregation_info();

                // Constructs aggregate packets
                virtual rtp_error_t add_aggregate_packet(uint8_t* data, size_t data_len);

                // constructs h265 RTP header with correct values
                virtual rtp_error_t fu_division(uint8_t* data, size_t data_len, size_t payload_size);

                /* Gets the format specific nal type from data*/
                virtual uint8_t get_nal_type(uint8_t* data) const;

                virtual uint8_t get_payload_header_size() const;
                virtual uint8_t get_nal_header_size() const;
                virtual uint8_t get_fu_header_size() const;
                virtual uint8_t get_start_code_range() const;
                virtual uvgrtp::formats::FRAG_TYPE get_fragment_type(uvgrtp::frame::rtp_frame* frame) const;
                virtual uvgrtp::formats::NAL_TYPE  get_nal_type(uvgrtp::frame::rtp_frame* frame) const;

                virtual void get_nal_header_from_fu_headers(size_t fptr, uint8_t* frame_payload, uint8_t* complete_payload);

            private:
                h265_aggregation_packet aggr_pkt_info_;
        };
    }
}

namespace uvg_rtp = uvgrtp;
