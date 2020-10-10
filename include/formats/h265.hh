#pragma once

#include <deque>
#include <map>
#include <unordered_set>
#include <vector>

#include "frame.hh"
#include "queue.hh"
#include "formats/h26x.hh"

namespace uvg_rtp {

    namespace formats {

        enum H265_NAL_TYPES {
            H265_PKT_AGGR = 48,
            H265_PKT_FRAG = 49
        };

        struct h265_aggregation_packet {
            uint8_t nal_header[uvg_rtp::frame::HEADER_SIZE_H265_NAL];
            uvg_rtp::buf_vec nalus;  /* discrete NAL units */
            uvg_rtp::buf_vec aggr_pkt; /* crafted aggregation packet */
        };

        struct h265_headers {
            uint8_t nal_header[uvg_rtp::frame::HEADER_SIZE_H265_NAL];

            /* there are three types of Fragmentation Unit headers:
             *  - header for the first fragment
             *  - header for all middle fragments
             *  - header for the last fragment */
            uint8_t fu_headers[3 * uvg_rtp::frame::HEADER_SIZE_H265_FU];
        };

        typedef struct h265_info {
            /* clock reading when the first fragment is received */
            uvg_rtp::clock::hrc::hrc_t sframe_time;

            /* sequence number of the frame with s-bit */
            uint32_t s_seq;

            /* sequence number of the frame with e-bit */
            uint32_t e_seq;

            /* how many fragments have been received */
            size_t pkts_received;

            /* total size of all fragments */
            size_t total_size;

            /* map of frame's fragments,
             * allows out-of-order insertion and loop-through in order */
            std::map<uint32_t, uvg_rtp::frame::rtp_frame *> fragments;

            /* storage for fragments that require relocation */
            std::vector<uvg_rtp::frame::rtp_frame *> temporary;
        } h265_info_t;

        typedef struct {
            std::deque<uvg_rtp::frame::rtp_frame *> queued;
            std::unordered_map<uint32_t, h265_info_t> frames;
            std::unordered_set<uint32_t> dropped;
        } h265_frame_info_t;

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

                /* If the packet handler must return more than one frame, it can install a frame getter
                 * that is called by the auxiliary handler caller if packet_handler() returns RTP_MULTIPLE_PKTS_READY
                 *
                 * "arg" is the same that is passed to packet_handler
                 *
                 * Return RTP_PKT_READY if "frame" contains a frame that can be returned to user
                 * Return RTP_NOT_FOUND if there are no more frames */
                static rtp_error_t frame_getter(void *arg, frame::rtp_frame **frame);

                /* Return pointer to the internal frame info structure which is relayed to packet handler */
                h265_frame_info_t *get_h265_frame_info();

            protected:
                rtp_error_t push_nal_unit(uint8_t *data, size_t data_len, bool more);

            private:
                /* Construct an aggregation packet from data in "aggr_pkt_info_" */
                rtp_error_t make_aggregation_pkt();

                /* Clear aggregation buffers */
                void clear_aggregation_info();

                h265_frame_info_t finfo_;
                h265_aggregation_packet aggr_pkt_info_;
        };
    };
};
