#pragma once

#include "h26x.hh"
#include "clock.hh"
#include "util.hh"
#include "frame.hh"
#include "socket.hh"

#include <deque>
#include <map>
#include <unordered_set>
#include <vector>

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

        typedef struct h265_info {
            /* clock reading when the first fragment is received */
            uvgrtp::clock::hrc::hrc_t sframe_time;

            /* sequence number of the frame with s-bit */
            uint32_t s_seq = 0;

            /* sequence number of the frame with e-bit */
            uint32_t e_seq = 0;

            /* how many fragments have been received */
            size_t pkts_received = 0;

            /* total size of all fragments */
            size_t total_size = 0;

            /* map of frame's fragments,
             * allows out-of-order insertion and loop-through in order */
            std::map<uint32_t, uvgrtp::frame::rtp_frame *> fragments;

            /* storage for fragments that require relocation */
            std::vector<uvgrtp::frame::rtp_frame *> temporary;
        } h265_info_t;

        typedef struct {
            std::deque<uvgrtp::frame::rtp_frame *> queued;
            std::unordered_map<uint32_t, h265_info_t> frames;
            std::unordered_set<uint32_t> dropped;
            uvgrtp::rtp *rtp_ctx; // cannot be initialized because struct unnamed
        } h265_frame_info_t;

        class h265 : public h26x {
            public:
                h265(uvgrtp::socket *socket, uvgrtp::rtp *rtp, int flags);
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
                // get H265 nal type
                virtual uint8_t get_nal_type(uint8_t* data);

                /* Construct an aggregation packet from data in "aggr_pkt_info_" */
                virtual rtp_error_t make_aggregation_pkt();

                /* Clear aggregation buffers */
                virtual void clear_aggregation_info();

                // Constructs aggregate packets
                virtual rtp_error_t handle_small_packet(uint8_t* data, size_t data_len, bool more);

                // constructs h265 RTP header with correct values
                virtual rtp_error_t construct_format_header_divide_fus(uint8_t* data, size_t& data_left, 
                    size_t& data_pos, size_t payload_size, uvgrtp::buf_vec& buffers);

            private:
                h265_frame_info_t finfo_;
                h265_aggregation_packet aggr_pkt_info_;
        };
    };
};

namespace uvg_rtp = uvgrtp;
