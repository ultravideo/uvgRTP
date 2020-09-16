#pragma once

#include <map>
#include <unordered_set>

#include "frame.hh"
#include "media.hh"
#include "queue.hh"

namespace uvg_rtp {

    namespace formats {

        struct hevc_headers {
            uint8_t nal_header[uvg_rtp::frame::HEADER_SIZE_HEVC_NAL];

            /* there are three types of Fragmentation Unit headers:
             *  - header for the first fragment
             *  - header for all middle fragments
             *  - header for the last fragment */
            uint8_t fu_headers[3 * uvg_rtp::frame::HEADER_SIZE_HEVC_FU];
        };

        typedef struct hevc_info {
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
            std::map<uint16_t, uvg_rtp::frame::rtp_frame *> fragments;
        } hevc_info_t;

        typedef struct {
            std::unordered_map<uint32_t, hevc_info_t> frames;
            std::unordered_set<uint32_t> dropped;
        } hevc_frame_info_t;

        class hevc : public media {
            public:
                hevc(uvg_rtp::socket *socket, uvg_rtp::rtp *rtp, int flags);
                ~hevc();

                /* Packet handler for RTP frames that transport HEVC bitstream
                 *
                 * Depending on what is received, packet handler will either merge
                 * TODO explain what it does on a high level
                 *
                 * Return RTP_OK if the packet was successfully handled
                 * Return RTP_PKT_READY if "out" contains an RTP that can be returned to user
                 * Return RTP_PKT_NOT_HANDLED if the packet is not handled by this handler
                 * Return RTP_PKT_MODIFIED if the packet was modified but should be forwarded to other handlers
                 * Return RTP_GENERIC_ERROR if the packet was corrupted in some way */
                static rtp_error_t packet_handler(void *arg, int flags, frame::rtp_frame **frame);

                /* Return pointer to the internal frame info structure which is relayed to packet handler */
                hevc_frame_info_t *get_hevc_frame_info();

            protected:
                rtp_error_t __push_frame(uint8_t *data, size_t data_len, int flags);

            private:
                rtp_error_t push_hevc_frame(uint8_t *data, size_t data_len);
                rtp_error_t push_hevc_nal(uint8_t *data, size_t data_len, bool more);

                hevc_frame_info_t finfo_;
        };
    };
};
