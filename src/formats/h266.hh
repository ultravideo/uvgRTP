#pragma once

#include "h26x.hh"

#include "uvgrtp/util.hh"
#include "uvgrtp/socket.hh"
#include "uvgrtp/clock.hh"
#include "uvgrtp/frame.hh"

#include <deque>
#include <memory>

namespace uvgrtp {

    class rtp;

    namespace formats {

        enum H266_NAL_TYPES {
            H266_PKT_FRAG = 29
        };

        struct h266_aggregation_packet {
            uint8_t payload_header[uvgrtp::frame::HEADER_SIZE_H266_PAYLOAD];
            uvgrtp::buf_vec nalus;  /* discrete NAL units */
            uvgrtp::buf_vec aggr_pkt; /* crafted aggregation packet */
        };

        struct h266_headers {
            uint8_t payload_header[uvgrtp::frame::HEADER_SIZE_H266_PAYLOAD];

            /* there are three types of Fragmentation Unit headers:
             *  - header for the first fragment
             *  - header for all middle fragments
             *  - header for the last fragment */
            uint8_t fu_headers[3 * uvgrtp::frame::HEADER_SIZE_H266_FU];
        };

        class h266 : public h26x {
            public:
                h266(std::shared_ptr<uvgrtp::socket> socket, std::shared_ptr<uvgrtp::rtp> rtp, int flags);
                ~h266();

            protected:

                // constructs h266 RTP header with correct values
                virtual rtp_error_t construct_format_header_divide_fus(uint8_t* data, size_t data_len,
                    size_t payload_size, uvgrtp::buf_vec& buffers);

                virtual uint8_t get_nal_type(uint8_t* data) const;

                virtual uint8_t get_payload_header_size() const;
                virtual uint8_t get_nal_header_size() const;
                virtual uint8_t get_fu_header_size() const;
                virtual uint8_t get_start_code_range() const;
                virtual int get_fragment_type(uvgrtp::frame::rtp_frame* frame) const;
                virtual uvgrtp::formats::NAL_TYPES get_nal_type(uvgrtp::frame::rtp_frame* frame) const;
        };
    }
}

namespace uvg_rtp = uvgrtp;
