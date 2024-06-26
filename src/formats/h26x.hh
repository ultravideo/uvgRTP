#pragma once

#include "uvgrtp/util.hh"
#include "uvgrtp/clock.hh"
#include "uvgrtp/frame.hh"

#include "media.hh"
#include "../socket.hh"

#include <deque>
#include <memory>
#include <set>
#include <unordered_set>
#ifdef _WIN32
#include <ws2def.h>
#include <ws2ipdef.h>
#else
#include <netinet/in.h>
#endif

namespace uvgrtp {

    // forward definitions
    class rtp;

    namespace formats {

        enum class FRAG_TYPE {
            FT_INVALID = -2, /* invalid combination of S and E bits */
            FT_NOT_FRAG = -1, /* frame doesn't contain fragment */
            FT_START = 1, /* frame contains a fragment with S bit set */
            FT_MIDDLE = 2, /* frame is fragment but not S or E fragment */
            FT_END = 3, /* frame contains a fragment with E bit set */
            FT_AGGR = 4,  /* aggregation packet (without DON) */
            FT_STAP_B = 5 /* aggregation packet, H264 STAP-B, RFC 6184 5.7.1 */
        };

        enum class NAL_TYPE {
            NT_INTRA = 0x00,
            NT_INTER = 0x01,
            NT_OTHER = 0xff
        };

        struct frag_info {
            bool start = false;
            bool end = false;
            bool reconstructed = false;
        };

        /* Used for determining if the NAL unit can be reconstructed */
        struct nal {
            bool complete = false;
            std::set<uint16_t> seqs = {};
        };

        struct access_unit_info {
            /* clock reading when the first fragment is received */
            uvgrtp::clock::hrc::hrc_t sframe_time;

            /* total size of all fragments */
            size_t total_size = 0;

            /* Keep track of all sequence numbers for an access unit, as well as the ones that have not been yet used for reconstruction */
            std::set<uint16_t> received_packet_seqs;
            std::set<uint16_t> incomplete_packet_seqs;

            /* Save frag_info for each fragment in an access unit */
            std::unordered_map<uint16_t, frag_info> fragments_info;
        };

        struct nal_info
        {
            size_t offset = 0;
            size_t prefix_len = 0;
            size_t size = 0;
            bool aggregate = false;
            bool was_aggregated = false;
        };

        class h26x : public media {
            public:
                h26x(std::shared_ptr<uvgrtp::socket> socket, std::shared_ptr<uvgrtp::rtp> rtp, int rce_flags);
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
                rtp_error_t push_media_frame(sockaddr_in& addr, sockaddr_in6& addr6, uint8_t *data, size_t data_len, int rtp_flags, uint32_t ssrc);

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
                 * If "frame" is not a fragmentation unit, packet handler returns the packet
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
                rtp_error_t packet_handler(void* args, int rce_flags, uint8_t* read_ptr, size_t size, uvgrtp::frame::rtp_frame** out);

            protected:

                /* Handles small packets. May support aggregate packets or not*/
                /* Construct/clear aggregation packets.
                 * Default implementation does nothing. If aggregation_pkt is supported, the
                 * child class should change the behavior */
                virtual rtp_error_t add_aggregate_packet(uint8_t* data, size_t data_len);
                virtual rtp_error_t finalize_aggregation_pkt();
                virtual void clear_aggregation_info();

                rtp_error_t single_nal_unit(uint8_t* data, size_t data_len);

                // constructs format specific RTP header with correct values
                virtual rtp_error_t fu_division(uint8_t* data, size_t data_len, size_t payload_size) = 0;

                // a helper function that handles the fu division.
                rtp_error_t divide_frame_to_fus(uint8_t* data, size_t& data_left, size_t payload_size,
                    uvgrtp::buf_vec& buffers, uint8_t fu_headers[]);

                void initialize_fu_headers(uint8_t nal_type, uint8_t fu_headers[]);

                rtp_error_t handle_aggregation_packet(uvgrtp::frame::rtp_frame** out, uint8_t nal_header_size, int rce_flags);

                /* Gets the format specific nal type from data*/
                virtual uint8_t get_nal_type(uint8_t* data) const = 0;

                virtual uint8_t get_payload_header_size() const = 0;
                virtual uint8_t get_nal_header_size() const = 0;
                virtual uint8_t get_fu_header_size() const = 0;
                virtual uint8_t get_start_code_range() const = 0;
                virtual uvgrtp::formats::FRAG_TYPE get_fragment_type(uvgrtp::frame::rtp_frame* frame) const = 0;
                virtual uvgrtp::formats::NAL_TYPE  get_nal_type(uvgrtp::frame::rtp_frame* frame) const = 0;

                virtual void get_nal_header_from_fu_headers(size_t fptr, uint8_t* frame_payload, uint8_t* complete_payload) = 0;

                virtual uvgrtp::frame::rtp_frame* allocate_rtp_frame_with_startcode(bool add_start_code,
                    uvgrtp::frame::rtp_header& header, size_t payload_size_without_startcode, size_t& fptr);

                virtual void prepend_start_code(int rce_flags, uvgrtp::frame::rtp_frame** out);

        private:
            size_t drop_access_unit(uint32_t ts);

            inline uint16_t next_seq_num(uint16_t seq);
            inline void initialize_new_access_unit(uint32_t ts);

            void free_fragment(uint16_t sequence_number);

            void scl(uint8_t* data, size_t data_len, size_t packet_size, 
                std::vector<nal_info>& nals, bool& can_be_aggregated);

            void garbage_collect_lost_frames(size_t timout);

            rtp_error_t reconstruction(uvgrtp::frame::rtp_frame** out, size_t nal_size,
                int rce_flags, uint16_t s_seq, uint16_t e_seq, const uint8_t sizeof_fu_headers);

            bool is_duplicate_frame(uint32_t timestamp, uint16_t seq_num);

            uint16_t nextSeq(uint16_t currentSeq, std::set<uint16_t>& fragments);

            std::deque<uvgrtp::frame::rtp_frame*> queued_;
            std::unordered_map<uint32_t, access_unit_info> access_units_;

            // Save received RTP frame stats, used to check for duplicates in is_duplicate_frame()
            struct pkt_stats {
                uint32_t ts;
                uint16_t seq;
            };
            std::deque<pkt_stats> received_frames_;
            std::unordered_map<uint32_t, std::vector<uint16_t>> received_info_;

            // Holds all possible fragments with sequence number
            std::unordered_map<uint16_t, uvgrtp::frame::rtp_frame*> fragments_;

            // keep track of old, dropped access units so we don't accept invalid fragments
            std::unordered_map<uint32_t, uvgrtp::clock::hrc::hrc_t> dropped_ts_;
            /* Keep track of the order of dropped access units, so we can delete the oldest ones to not reserve increasing amounts
            of memory */
            std::set<uint32_t> dropped_in_order_;

            std::shared_ptr<uvgrtp::rtp> rtp_ctx_;

            uvgrtp::clock::hrc::hrc_t last_garbage_collection_;

            bool discard_until_key_frame_ = true;
        };
    }
}

namespace uvg_rtp = uvgrtp;
