#pragma once

#include "frame.hh"
#include "queue.hh"
#include "formats/h26x.hh"

namespace uvg_rtp {

    namespace formats {

        struct h265_headers {
            uint8_t nal_header[uvg_rtp::frame::HEADER_SIZE_H265_NAL];

            /* there are three types of Fragmentation Unit headers:
             *  - header for the first fragment
             *  - header for all middle fragments
             *  - header for the last fragment */
            uint8_t fu_headers[3 * uvg_rtp::frame::HEADER_SIZE_H265_FU];
        };

        class h265 : public h26x {
            public:
                h265(uvg_rtp::socket *socket, uvg_rtp::rtp *rtp, int flags);
                ~h265();

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
