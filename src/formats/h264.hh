#pragma once

#include "h26x.hh"

#include "uvgrtp/clock.hh"
#include "uvgrtp/util.hh"
#include "uvgrtp/frame.hh"

#include "socket.hh"

#include <deque>
#include <memory>

namespace uvgrtp {

    class rtp;

    namespace formats {

        constexpr uint8_t HEADER_SIZE_H264_INDICATOR = 1;
        constexpr uint8_t HEADER_SIZE_H264_NAL = 1;
        constexpr uint8_t HEADER_SIZE_H264_FU = 1;

        enum H264_NAL_TYPES {
            H264_NON_IDR = 1,
            H264_IDR = 5,
            H264_STAP_A = 24,
            H264_STAP_B   = 25,
            H264_PKT_FRAG = 28
        };

        struct h264_aggregation_packet {
            uint8_t fu_indicator[HEADER_SIZE_H264_INDICATOR] = {0};
            uvgrtp::buf_vec nalus;  /* discrete NAL units */
            uvgrtp::buf_vec aggr_pkt; /* crafted aggregation packet */
        };

        struct h264_headers {
            uint8_t fu_indicator[HEADER_SIZE_H264_INDICATOR];

            /* there are three types of Fragmentation Unit headers:
             *  - header for the first fragment
             *  - header for all middle fragments
             *  - header for the last fragment */
            uint8_t fu_headers[3 * HEADER_SIZE_H264_FU];
        };

        class h264 : public h26x {
            public:
                h264(std::shared_ptr<uvgrtp::socket> socket, std::shared_ptr<uvgrtp::rtp> rtp, int rce_flags);
                ~h264();

            protected:
                
                /* Construct an aggregation packet from data in "aggr_pkt_info_" 
                 * TODO: The code exists, but it is not used */
                virtual rtp_error_t finalize_aggregation_pkt();

                /* Clear aggregation buffers */
                virtual void clear_aggregation_info();

                // Constructs aggregate packets
                virtual rtp_error_t add_aggregate_packet(uint8_t* data, size_t data_len);

                // constructs h264 RTP header with correct values
                virtual rtp_error_t fu_division(uint8_t* data, size_t data_len, size_t payload_size);

                // get h264 nal type
                virtual uint8_t get_nal_type(uint8_t* data) const;

                virtual uint8_t get_payload_header_size() const;
                virtual uint8_t get_nal_header_size() const;
                virtual uint8_t get_fu_header_size() const;
                virtual uint8_t get_start_code_range() const;

                virtual uvgrtp::formats::FRAG_TYPE get_fragment_type(uvgrtp::frame::rtp_frame* frame) const;
                virtual uvgrtp::formats::NAL_TYPE  get_nal_type(uvgrtp::frame::rtp_frame* frame) const;

                virtual void get_nal_header_from_fu_headers(size_t fptr, uint8_t* frame_payload, uint8_t* complete_payload);

                virtual uvgrtp::frame::rtp_frame* allocate_rtp_frame_with_startcode(bool add_start_code,
                    uvgrtp::frame::rtp_header& header, size_t payload_size_without_startcode, size_t& fptr);

                virtual void prepend_start_code(int rce_flags, uvgrtp::frame::rtp_frame** out);

            private:
                h264_aggregation_packet aggr_pkt_info_;
        };
    }
}

namespace uvg_rtp = uvgrtp;
