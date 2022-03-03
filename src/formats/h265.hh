#pragma once

#include "h26x.hh"

#include "uvgrtp/clock.hh"
#include "uvgrtp/util.hh"
#include "uvgrtp/frame.hh"
#include "uvgrtp/socket.hh"

#include <deque>
#include <map>
#include <unordered_set>
#include <vector>
#include <memory>

namespace uvgrtp {

    namespace formats {

        enum H265_NAL_TYPES {
            H265_PKT_AGGR = 48,
            H265_PKT_FRAG = 49
        };

        struct h265_aggregation_packet {
            uint8_t nal_header[uvgrtp::frame::HEADER_SIZE_H265_NAL];
            uvgrtp::buf_vec nalus;  /* discrete NAL units */
            uvgrtp::buf_vec aggr_pkt; /* crafted aggregation packet */
        };

        struct h265_headers {
            uint8_t nal_header[uvgrtp::frame::HEADER_SIZE_H265_NAL];

            /* there are three types of Fragmentation Unit headers:
             *  - header for the first fragment
             *  - header for all middle fragments
             *  - header for the last fragment */
            uint8_t fu_headers[3 * uvgrtp::frame::HEADER_SIZE_H265_FU];
        };

        class h265 : public h26x {
            public:
                h265(uvgrtp::socket *socket, std::shared_ptr<uvgrtp::rtp> rtp, int flags);
                ~h265();

            protected:
                /* Construct an aggregation packet from data in "aggr_pkt_info_" */
                virtual rtp_error_t make_aggregation_pkt();

                /* Clear aggregation buffers */
                virtual void clear_aggregation_info();

                // Constructs aggregate packets
                virtual rtp_error_t handle_small_packet(uint8_t* data, size_t data_len, bool more);

                // constructs h265 RTP header with correct values
                virtual rtp_error_t construct_format_header_divide_fus(uint8_t* data, size_t& data_left, 
                    size_t& data_pos, size_t payload_size, uvgrtp::buf_vec& buffers);

                /* Gets the format specific nal type from data*/
                virtual uint8_t get_nal_type(uint8_t* data) const;

                virtual uint8_t get_nal_header_size() const;
                virtual uint8_t get_fu_header_size() const;
                virtual uint8_t get_start_code_range() const;
                virtual int get_fragment_type(uvgrtp::frame::rtp_frame* frame) const;
                virtual uvgrtp::formats::NAL_TYPES get_nal_type(uvgrtp::frame::rtp_frame* frame) const;

            private:
                h265_aggregation_packet aggr_pkt_info_;
        };
    }
}

namespace uvg_rtp = uvgrtp;
