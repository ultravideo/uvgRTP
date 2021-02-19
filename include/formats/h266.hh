#pragma once

#include "frame.hh"
#include "queue.hh"
#include "formats/h26x.hh"

namespace uvgrtp {

    namespace formats {

        struct h266_headers {
            uint8_t nal_header[uvgrtp::frame::HEADER_SIZE_H266_NAL];

            /* there are three types of Fragmentation Unit headers:
             *  - header for the first fragment
             *  - header for all middle fragments
             *  - header for the last fragment */
            uint8_t fu_headers[3 * uvgrtp::frame::HEADER_SIZE_H266_FU];
        };

        class h266 : public h26x {
            public:
                h266(uvgrtp::socket *socket, uvgrtp::rtp *rtp, int flags);
                ~h266();

                /* Packet handler for RTP frames that transport HEVC bitstream
                 *
                 * If "frame" is not a fragmentation unit, packet handler checks
                 * if "frame" is SPS/VPS/PPS packet and if so, returns the packet
                 * to user immediately.
                 *
                 * If "frame" is a fragmentation unit, packet handler checks if
                 * it has received all fragments of a complete HEVC NAL unit and if
                 * so, it merges all fragments into a complete NAL unit and returns
                 * the NAL unit to user. If the NAL unit is not complete, packet
                 * handler holds onto the frame and waits for other fragments to arrive.
                 *
                 * Return RTP_OK if the packet was successfully handled
                 * Return RTP_PKT_READY if "frame" contains an RTP that can be returned to user
                 * Return RTP_PKT_NOT_HANDLED if the packet is not handled by this handler
                 * Return RTP_PKT_MODIFIED if the packet was modified but should be forwarded to other handlers
                 * Return RTP_GENERIC_ERROR if the packet was corrupted in some way */
                static rtp_error_t packet_handler(void *arg, int flags, frame::rtp_frame **frame);

            protected:
                rtp_error_t push_nal_unit(uint8_t *data, size_t data_len, bool more);
        };
    };
};

namespace uvg_rtp = uvgrtp;
