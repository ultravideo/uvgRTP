#pragma once

#include "media.hh"
#include "uvgrtp/util.hh"
#include "uvgrtp/socket.hh"
#include "uvgrtp/clock.hh"
#include "uvgrtp/frame.hh"

#include <deque>
#include <memory>

namespace uvgrtp {

    // forward definitions
    class rtp;

    namespace formats {

        #define INVALID_SEQ           0x13371338
        #define RTP_HDR_SIZE  12

        enum FRAG_TYPES {
            FT_INVALID = -2, /* invalid combination of S and E bits */
            FT_NOT_FRAG = -1, /* frame doesn't contain fragment */
            FT_START = 1, /* frame contains a fragment with S bit set */
            FT_MIDDLE = 2, /* frame is fragment but not S or E fragment */
            FT_END = 3, /* frame contains a fragment with E bit set */
            FT_AGGR = 4  /* aggregation packet */
        };

        enum NAL_TYPES {
            NT_INTRA = 0x00,
            NT_INTER = 0x01,
            NT_OTHER = 0xff
        };

        typedef struct h26x_info {
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
            std::map<uint32_t, uvgrtp::frame::rtp_frame*> fragments;

            /* storage for fragments that require relocation */
            std::vector<uvgrtp::frame::rtp_frame*> temporary;
        } h26x_info_t;

        class h26x : public media {
            public:
                h26x(std::shared_ptr<uvgrtp::socket> socket, std::shared_ptr<uvgrtp::rtp> rtp, int flags);
                virtual ~h26x();

                /* Find H26x start code from "data"
                 * This process is the same for H26{4,5,6}
                 *
                 * Return the offset of the start code on success
                 * Return -1 if no start code was found */
                ssize_t find_h26x_start_code(uint8_t *data, size_t len, size_t offset, uint8_t& start_len);

                /* Top-level push_frame() called by the Media class
                 * Sets up the frame queue for the send operation
                 *
                 * Return RTP_OK on success
                 * Return RTP_INVALID_VALUE if one of the parameters is invalid */
                rtp_error_t push_media_frame(uint8_t *data, size_t data_len, int flags);

                /* Last push_frame() on the call stack which splits the input frame ("data")
                 * into NAL units using find_h26x_start_code() and fragments the NAL unit
                 * into Fragmentation Units (FUs) which are pushed to frame queue
                 *
                 * Return RTP_OK on success
                 * Return RTP_INVALID_VALUE if one the parameters is invalid */
                rtp_error_t push_h26x_frame(uint8_t *data, size_t data_len, int flags);

                /* If the packet handler must return more than one frame, it can install a frame getter
                 * that is called by the auxiliary handler caller if packet_handler() returns RTP_MULTIPLE_PKTS_READY
                 *
                 * "arg" is the same that is passed to packet_handler
                 *
                 * Return RTP_PKT_READY if "frame" contains a frame that can be returned to user
                 * Return RTP_NOT_FOUND if there are no more frames */
                rtp_error_t frame_getter(frame::rtp_frame** frame);

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
                rtp_error_t packet_handler(int flags, frame::rtp_frame** frame);

            protected:

                /* Handles small packets. May support aggregate packets or not*/
                virtual rtp_error_t handle_small_packet(uint8_t* data, size_t data_len, bool more) = 0;

                uvgrtp::frame::rtp_frame* allocate_rtp_frame_with_startcode(bool add_start_code,
                    uvgrtp::frame::rtp_header& header, size_t payload_size_without_startcode, size_t& fptr);
                static void prepend_start_code(int flags, uvgrtp::frame::rtp_frame** out);

                // constructs format specific RTP header with correct values
                virtual rtp_error_t construct_format_header_divide_fus(uint8_t* data, size_t& data_left,
                    size_t& data_pos, size_t payload_size, uvgrtp::buf_vec& buffers) = 0;

                /* Construct/clear aggregation packets.
                 * Default implementation does nothing. If aggregation_pkt is supported, the 
                 * child class should change the behavior */
                virtual rtp_error_t make_aggregation_pkt();
                virtual void clear_aggregation_info();

                // a helper function that handles the fu division.
                rtp_error_t divide_frame_to_fus(uint8_t* data, size_t& data_left, size_t& data_pos, size_t payload_size,
                    uvgrtp::buf_vec& buffers, uint8_t fu_headers[]);

                void initialize_fu_headers(uint8_t nal_type, uint8_t fu_headers[]);

                bool is_frame_late(uvgrtp::formats::h26x_info_t& hinfo, size_t max_delay);

                uint32_t drop_frame(uint32_t ts);

                rtp_error_t handle_aggregation_packet(uvgrtp::frame::rtp_frame** out, uint8_t nal_header_size, int flags);

                /* Gets the format specific nal type from data*/
                virtual uint8_t get_nal_type(uint8_t* data) const = 0;

                virtual uint8_t get_nal_header_size() const = 0;
                virtual uint8_t get_fu_header_size() const = 0;
                virtual uint8_t get_start_code_range() const = 0;
                virtual int get_fragment_type(uvgrtp::frame::rtp_frame* frame) const = 0;
                virtual uvgrtp::formats::NAL_TYPES get_nal_type(uvgrtp::frame::rtp_frame* frame) const = 0;

                virtual void copy_nal_header(size_t fptr, uint8_t* frame_payload, uint8_t* complete_payload);

        private:

            inline size_t calculate_expected_fus(uint32_t ts);
            inline void initialize_new_fragmented_frame(uint32_t ts);

            // constructs and sends the RTP packets with format specific stuff
            rtp_error_t push_nal_unit(uint8_t* data, size_t data_len, bool more);

            void garbage_collect_lost_frames();

            std::deque<uvgrtp::frame::rtp_frame*> queued_;
            std::unordered_map<uint32_t, h26x_info_t> frames_;
            std::unordered_set<uint32_t> dropped_;
            std::shared_ptr<uvgrtp::rtp> rtp_ctx_;

            uvgrtp::clock::hrc::hrc_t last_garbage_collection_;
        };
    }
}

namespace uvg_rtp = uvgrtp;
