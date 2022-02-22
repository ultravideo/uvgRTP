#pragma once

#include "h26x.hh"

#include "uvgrtp/clock.hh"
#include "uvgrtp/util.hh"
#include "uvgrtp/frame.hh"
#include "uvgrtp/socket.hh"

#include <deque>

namespace uvgrtp {

    class rtp;

    namespace formats {

        enum H264_NAL_TYPES {
            H264_PKT_AGGR = 24,
            H264_PKT_FRAG = 28
        };

        struct h264_aggregation_packet {
            uint8_t fu_indicator[uvgrtp::frame::HEADER_SIZE_H264_FU];
            uvgrtp::buf_vec nalus;  /* discrete NAL units */
            uvgrtp::buf_vec aggr_pkt; /* crafted aggregation packet */
        };

        struct h264_headers {
            uint8_t fu_indicator[uvgrtp::frame::HEADER_SIZE_H264_FU];

            /* there are three types of Fragmentation Unit headers:
             *  - header for the first fragment
             *  - header for all middle fragments
             *  - header for the last fragment */
            uint8_t fu_headers[3 * uvgrtp::frame::HEADER_SIZE_H264_FU];
        };

        class h264 : public h26x {
            public:
                h264(uvgrtp::socket *socket, uvgrtp::rtp *rtp, int flags);
                ~h264();

            protected:

                // the aggregation packet is not enabled
                virtual rtp_error_t handle_small_packet(uint8_t* data, size_t data_len, bool more);
                
                /* Construct an aggregation packet from data in "aggr_pkt_info_" 
                 * TODO: The code exists, but it is not used */
                virtual rtp_error_t make_aggregation_pkt();

                /* Clear aggregation buffers */
                virtual void clear_aggregation_info();

                // constructs h264 RTP header with correct values
                virtual rtp_error_t construct_format_header_divide_fus(uint8_t* data, size_t& data_left, 
                    size_t& data_pos, size_t payload_size,  uvgrtp::buf_vec& buffers);

                // get h264 nal type
                virtual uint8_t get_nal_type(uint8_t* data) const;

                virtual uint8_t get_nal_header_size() const;
                virtual uint8_t get_fu_header_size() const;

                virtual int get_fragment_type(uvgrtp::frame::rtp_frame* frame) const;
                virtual uvgrtp::formats::NAL_TYPES get_nal_type(uvgrtp::frame::rtp_frame* frame) const;

                virtual void copy_nal_header(size_t fptr, uint8_t* frame_payload, uint8_t* complete_payload);

            private:
                h264_aggregation_packet aggr_pkt_info_;
        };
    };
};

namespace uvg_rtp = uvgrtp;
