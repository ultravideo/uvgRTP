#pragma once

#include "clock.hh"
#include "util.hh"
#include "socket.hh"
#include "frame.hh"
#include "runner.hh"


#include <bitset>
#include <map>
#include <thread>
#include <vector>
#include <functional>
#include <memory>

namespace uvgrtp {

    class rtp;
    class srtcp;

    /// \cond DO_NOT_DOCUMENT
    enum RTCP_ROLE {
        RECEIVER,
        SENDER
    };

    /* TODO: explain these constants */
    const uint32_t RTP_SEQ_MOD    = 1 << 16;
    const uint32_t MIN_SEQUENTIAL = 2;
    const uint32_t MAX_DROPOUT    = 3000;
    const uint32_t MAX_MISORDER   = 100;
    const uint32_t MIN_TIMEOUT    = 5000;

    struct rtcp_statistics {
        /* receiver stats */
        uint32_t received_pkts = 0;  /* Number of packets received */
        uint32_t dropped_pkts = 0;   /* Number of dropped RTP packets */
        uint32_t received_bytes = 0; /* Number of bytes received excluding RTP Header */

        /* sender stats */
        uint32_t sent_pkts = 0;      /* Number of sent RTP packets */
        uint32_t sent_bytes = 0;     /* Number of sent bytes excluding RTP Header */

        uint32_t jitter = 0;         /* TODO: */
        uint32_t transit = 0;        /* TODO: */

        /* Receiver clock related stuff */
        uint64_t initial_ntp = 0;    /* Wallclock reading when the first RTP packet was received */
        uint32_t initial_rtp = 0;    /* RTP timestamp of the first RTP packet received */
        uint32_t clock_rate = 0;     /* Rate of the clock (used for jitter calculations) */

        uint32_t lsr = 0;                /* Middle 32 bits of the 64-bit NTP timestamp of previous SR */
        uvgrtp::clock::hrc::hrc_t sr_ts; /* When the last SR was received (used to calculate delay) */

        uint16_t max_seq = 0;        /* Highest sequence number received */
        uint32_t base_seq = 0;       /* First sequence number received */
        uint32_t bad_seq = 0;        /* TODO:  */
        uint32_t cycles = 0;         /* Number of sequence cycles */
    };

    struct rtcp_participant {
        uvgrtp::socket *socket = nullptr; /* socket associated with this participant */
        sockaddr_in address;         /* address of the participant */
        struct rtcp_statistics stats; /* RTCP session statistics of the participant */

        uint32_t probation = 0;           /* has the participant been fully accepted to the session */
        int role = 0;                /* is the participant a sender or a receiver */

        /* Save the latest RTCP packets received from this participant
         * Users can query these packets using the SSRC of participant */
        uvgrtp::frame::rtcp_sender_report   *sr_frame = nullptr;
        uvgrtp::frame::rtcp_receiver_report *rr_frame = nullptr;
        uvgrtp::frame::rtcp_sdes_packet     *sdes_frame = nullptr;
        uvgrtp::frame::rtcp_app_packet      *app_frame = nullptr;
    };
    /// \endcond

    class rtcp : public runner {
        public:
            /// \cond DO_NOT_DOCUMENT
            rtcp(uvgrtp::rtp *rtp, int flags);
            rtcp(uvgrtp::rtp *rtp, uvgrtp::srtcp *srtcp, int flags);
            ~rtcp();

            /* start the RTCP runner thread
             *
             * return RTP_OK on success and RTP_MEMORY_ERROR if the allocation fails */
            rtp_error_t start();

            /* End the RTCP session and send RTCP BYE to all participants
             *
             * return RTP_OK on success */
            rtp_error_t stop();

            /* Generate either RTCP Sender or Receiver report and sent it to all participants
             * Return RTP_OK on success and RTP_ERROR on error */
            rtp_error_t generate_report();

            /* Handle incoming RTCP packet (first make sure it's a valid RTCP packet)
             * This function will call one of the above functions internally
             *
             * Return RTP_OK on success and RTP_ERROR on error */
            rtp_error_t handle_incoming_packet(uint8_t *buffer, size_t size);
            /// \endcond

            /* Send "frame" to all participants
             *
             * These routines will convert all necessary fields to network byte order
             *
             * Return RTP_OK on success
             * Return RTP_INVALID_VALUE if "frame" is in some way invalid
             * Return RTP_SEND_ERROR if sending "frame" did not succeed (see socket.hh for details) */

            /**
             * \brief Send an RTCP SDES packet
             *
             * \param items Vector of SDES items
             *
             * \retval RTP_OK On success
             * \retval RTP_MEMORY_ERROR If allocation fails
             * \retval RTP_GENERIC_ERROR If sending fails
             */
            rtp_error_t send_sdes_packet(std::vector<uvgrtp::frame::rtcp_sdes_item>& items);

            /**
             * \brief Send an RTCP APP packet
             *
             * \param name Name of the APP item, e.g., EMAIL or PHONE
             * \param subtype Subtype of the APP item
             * \param payload_len Length of the payload
             * \param payload Payload
             *
             * \retval RTP_OK On success
             * \retval RTP_MEMORY_ERROR If allocation fails
             * \retval RTP_GENERIC_ERROR If sending fails
             */
            rtp_error_t send_app_packet(char *name, uint8_t subtype, size_t payload_len, uint8_t *payload);

            /**
             * \brief Send an RTCP BYE packet
             *
             * \details In case the quitting participant is a mixer and is serving multiple
             * paricipants, the input vector contains the SSRCs of all those participants. If the
             * participant is a regular member of the session, the vector only contains the SSRC
             * of the participant.
             *
             * \param ssrcs Vector of SSRCs of those participants who are quitting
             *
             * \retval RTP_OK On success
             * \retval RTP_MEMORY_ERROR If allocation fails
             * \retval RTP_GENERIC_ERROR If sending fails
             */
            rtp_error_t send_bye_packet(std::vector<uint32_t> ssrcs);

            /// \cond DO_NOT_DOCUMENT
            /* Return the latest RTCP packet received from participant of "ssrc"
             * Return nullptr if we haven't received this kind of packet or if "ssrc" doesn't exist
             *
             * NOTE: Caller is responsible for deallocating the memory */
            uvgrtp::frame::rtcp_sender_report   *get_sender_packet(uint32_t ssrc);
            uvgrtp::frame::rtcp_receiver_report *get_receiver_packet(uint32_t ssrc);
            uvgrtp::frame::rtcp_sdes_packet     *get_sdes_packet(uint32_t ssrc);
            uvgrtp::frame::rtcp_app_packet      *get_app_packet(uint32_t ssrc);

            /* Return a reference to vector that contains the sockets of all participants */
            std::vector<uvgrtp::socket>& get_sockets();

            /* Somebody joined the multicast group the owner of this RTCP instance is part of
             * Add it to RTCP participant list so we can start listening for reports
             *
             * "clock_rate" tells how much the RTP timestamp advances, this information is needed
             * to calculate the interarrival jitter correctly. It has nothing do with our clock rate,
             * (or whether we're even sending anything)
             *
             * Return RTP_OK on success and RTP_ERROR on error */
            rtp_error_t add_participant(std::string dst_addr, uint16_t dst_port, uint16_t src_port, uint32_t clock_rate);

            /* Functions for updating various RTP sender statistics */
            void sender_inc_seq_cycle_count();
            void sender_inc_sent_pkts(size_t n);
            void sender_inc_sent_bytes(size_t n);
            void sender_update_stats(uvgrtp::frame::rtp_frame *frame);

            void receiver_inc_sent_bytes(uint32_t sender_ssrc, size_t n);
            void receiver_inc_overhead_bytes(uint32_t sender_ssrc, size_t n);
            void receiver_inc_total_bytes(uint32_t sender_ssrc, size_t n);
            void receiver_inc_sent_pkts(uint32_t sender_ssrc, size_t n);

            /* Update the RTCP statistics regarding this packet
             *
             * Return RTP_OK if packet is valid
             * Return RTP_INVALID_VALUE if SSRCs of remotes have collided or the packet is invalid in some way
             * return RTP_SSRC_COLLISION if our own SSRC has collided and we need to reinitialize it */
            rtp_error_t receiver_update_stats(uvgrtp::frame::rtp_frame *frame);

            /* If we've detected that our SSRC has collided with someone else's SSRC, we need to
             * generate new random SSRC and reinitialize our own RTCP state.
             * RTCP object still has the participants of "last session", we can use their SSRCs
             * to detected new collision
             *
             * Return RTP_OK if reinitialization succeeded
             * Return RTP_SSRC_COLLISION if our new SSRC has collided and we need to generate new SSRC */
            rtp_error_t reset_rtcp_state(uint32_t ssrc);

            /* Update various session statistics */
            void update_session_statistics(uvgrtp::frame::rtp_frame *frame);

            /* Return SSRCs of all participants */
            std::vector<uint32_t> get_participants();
            /// \endcond

            /**
             * \brief Provide timestamping information for RTCP
             *
             * \details If the application wishes to timestamp the stream itself AND it has
             * enabled RTCP by using ::RCE_RTCP, it must provide timestamping information for
             * RTCP so sensible synchronization values can be calculated for Sender Reports
             *
             * The application can call uvgrtp::clock::ntp::now() to get the current wall clock
             * reading as an NTP timestamp value
             *
             * \param clock_start NTP timestamp for t = 0
             * \param clock_rate Clock rate of the stream
             * \param rtp_ts_start RTP timestamp for t = 0
             */
            void set_ts_info(uint64_t clock_start, uint32_t clock_rate, uint32_t rtp_ts_start);

            /* Alternate way to get RTCP packets is to install a hook for them. So instead of
             * polling an RTCP packet, user can install a function that is called when
             * a specific RTCP packet is received. */

            /**
             * \brief Install an RTCP Sender Report hook
             *
             * \details This function is called when an RTCP Sender Report is received
             *
             * \param hook Function pointer to the hook
             *
             * \retval RTP_OK on success
             * \retval RTP_INVALID_VALUE If hook is nullptr
             */
            rtp_error_t install_sender_hook(void (*hook)(uvgrtp::frame::rtcp_sender_report *));
            rtp_error_t install_sender_hook(std::function<void(std::shared_ptr<uvgrtp::frame::rtcp_sender_report>)> sr_handler);

            /**
             * \brief Install an RTCP Receiver Report hook
             *
             * \details This function is called when an RTCP Receiver Report is received
             *
             * \param hook Function pointer to the hook
             *
             * \retval RTP_OK on success
             * \retval RTP_INVALID_VALUE If hook is nullptr
             */
            rtp_error_t install_receiver_hook(void (*hook)(uvgrtp::frame::rtcp_receiver_report *));
            rtp_error_t install_receiver_hook(std::function<void(std::shared_ptr<uvgrtp::frame::rtcp_receiver_report>)> rr_handler);

            /**
             * \brief Install an RTCP SDES packet hook
             *
             * \details This function is called when an RTCP SDES packet is received
             *
             * \param hook Function pointer to the hook
             *
             * \retval RTP_OK on success
             * \retval RTP_INVALID_VALUE If hook is nullptr
             */
            rtp_error_t install_sdes_hook(void (*hook)(uvgrtp::frame::rtcp_sdes_packet *));
            rtp_error_t install_sdes_hook(std::function<void(std::shared_ptr<uvgrtp::frame::rtcp_sdes_packet>)> sdes_handler);

            /**
             * \brief Install an RTCP APP packet hook
             *
             * \details This function is called when an RTCP APP packet is received
             *
             * \param hook Function pointer to the hook
             *
             * \retval RTP_OK on success
             * \retval RTP_INVALID_VALUE If hook is nullptr
             */
            rtp_error_t install_app_hook(void (*hook)(uvgrtp::frame::rtcp_app_packet *));
            rtp_error_t install_app_hook(std::function<void(std::shared_ptr<uvgrtp::frame::rtcp_app_packet>)> app_handler);

            /// \cond DO_NOT_DOCUMENT
            /* Update RTCP-related sender statistics */
            rtp_error_t update_sender_stats(size_t pkt_size);

            /* Update RTCP-related receiver statistics */
            static rtp_error_t recv_packet_handler(void *arg, int flags, frame::rtp_frame **out);

            /* Update RTCP-related sender statistics */
            static rtp_error_t send_packet_handler_vec(void *arg, uvgrtp::buf_vec& buffers);
            /// \endcond

        private:

            /* Handle different kinds of incoming rtcp packets. The read header is passed to functions
               which read rest of the frame type specific data.
             * Return RTP_OK on success and RTP_ERROR on error */
            rtp_error_t handle_sender_report_packet(uint8_t* frame, size_t size,
                uvgrtp::frame::rtcp_header& header);
            rtp_error_t handle_receiver_report_packet(uint8_t* frame, size_t size,
                uvgrtp::frame::rtcp_header& header);
            rtp_error_t handle_sdes_packet(uint8_t* frame, size_t size,
                uvgrtp::frame::rtcp_header& header);
            rtp_error_t handle_bye_packet(uint8_t* frame, size_t size);
            rtp_error_t handle_app_packet(uint8_t* frame, size_t size,
                uvgrtp::frame::rtcp_header& header);

            static void rtcp_runner(rtcp *rtcp);

            /* when we start the RTCP instance, we don't know what the SSRC of the remote is
             * when an RTP packet is received, we must check if we've already received a packet
             * from this sender and if not, create new entry to receiver_stats_ map */
            bool is_participant(uint32_t ssrc);

            /* When we receive an RTP or RTCP packet, we need to check the source address and see if it's
             * the same address where we've received packets before.
             *
             * If the address is new, it means we have detected an SSRC collision and the paket should
             * be dropped We also need to check whether this SSRC matches with our own SSRC and if it does
             * we need to send RTCP BYE and rejoin to the session */
            bool collision_detected(uint32_t ssrc, sockaddr_in& src_addr);

            /* Move participant from initial_peers_ to participants_ */
            rtp_error_t add_participant(uint32_t ssrc);

            /* We've got a message from new source (the SSRC of the frame is not known to us)
             * Initialize statistics for the peer and move it to participants_ */
            rtp_error_t init_new_participant(uvgrtp::frame::rtp_frame *frame);

            /* Initialize the RTP Sequence related stuff of peer
             * This function assumes that the peer already exists in the participants_ map */
            rtp_error_t init_participant_seq(uint32_t ssrc, uint16_t base_seq);

            /* Update the SSRC's sequence related data in participants_ map
             *
             * Return RTP_OK if the received packet was OK
             * Return RTP_GENERIC_ERROR if it wasn't and
             * packet-related statistics should not be updated */
            rtp_error_t update_participant_seq(uint32_t ssrc, uint16_t seq);

            /* Update the RTCP bandwidth variables
             *
             * "pkt_size" tells how much rtcp_byte_count_
             * should be increased before calculating the new average */
            void update_rtcp_bandwidth(size_t pkt_size);

            /* Because struct statistics contains uvgRTP clock object we cannot
             * zero it out without compiler complaining about it so all the fields
             * must be set to zero manually */
            void zero_stats(uvgrtp::rtcp_statistics *stats);

            /* Set the first four or eight bytes of an RTCP packet */
            rtp_error_t construct_rtcp_header(size_t packet_size, uint8_t*& frame,
                uint16_t secondField, uvgrtp::frame::RTCP_FRAME_TYPE frame_type, bool addLocalSSRC);

            /* read the header values from rtcp packet */
            void read_rtcp_header(uint8_t* packet, uvgrtp::frame::rtcp_header& header);
            void read_reports(uint8_t* packet, size_t size, uint8_t count, 
                std::vector<uvgrtp::frame::rtcp_report_block>& reports);

            /* Takes ownership of the frame */
            rtp_error_t send_rtcp_packet_to_participants(uint8_t* frame, size_t frame_size);

            /* Pointer to RTP context from which clock rate etc. info is collected and which is
             * used to change SSRC if a collision is detected */
            uvgrtp::rtp *rtp_;

            /* Secure RTCP context */
            uvgrtp::srtcp *srtcp_;

            /* RTP context flags */
            int flags_;

            /* are we a sender or a receiver */
            int our_role_;

            /* TODO: time_t?? */
            size_t tp_;       /* the last time an RTCP packet was transmitted */
            size_t tc_;       /* the current time */
            size_t tn_;       /* the next scheduled transmission time of an RTCP packet */
            size_t pmembers_; /* the estimated number of session members at the time tn was last recomputed */
            size_t members_;  /* the most current estimate for the number of session members */
            size_t senders_;  /* the most current estimate for the number of senders in the session */

            /* The target RTCP bandwidth, i.e., the total bandwidth
             * that will be used for RTCP packets by all members of this session,
             * in octets per second.  This will be a specified fraction of the
             * "session bandwidth" parameter supplied to the application at startup. */
            size_t rtcp_bandwidth_;

            /* Flag that is true if the application has sent data since
             * the 2nd previous RTCP report was transmitted. */
            bool we_sent_;

            /* The average compound RTCP packet size, in octets,
             * over all RTCP packets sent and received by this participant. The
             * size includes lower-layer transport and network protocol headers
             * (e.g., UDP and IP) as explained in Section 6.2 */
            size_t avg_rtcp_pkt_pize_;

            /* Number of RTCP packets and bytes sent and received by this participant */
            size_t rtcp_pkt_count_;
            size_t rtcp_byte_count_;

            /* Number of RTCP packets sent */
            uint32_t rtcp_pkt_sent_count_;

            /* Flag that is true if the application has not yet sent an RTCP packet. */
            bool initial_;

            /* Copy of our own current SSRC */
            uint32_t ssrc_;

            /* NTP timestamp associated with initial RTP timestamp (aka t = 0) */
            uint64_t clock_start_;

            /* Clock rate of the media ie. how fast does the time increase */
            uint32_t clock_rate_;

            /* The first value of RTP timestamp (aka t = 0) */
            uint32_t rtp_ts_start_;

            std::map<uint32_t, rtcp_participant *> participants_;
            uint8_t num_receivers_; // maximum is 32 (5 bits)

            /* statistics for RTCP Sender and Receiver Reports */
            struct rtcp_statistics our_stats;

            /* If we expect frames from remote but haven't received anything from remote yet,
             * the participant resides in this vector until he's moved to participants_ */
            std::vector<rtcp_participant *> initial_participants_;

            /* Vector of sockets the RTCP runner is listening to
             *
             * The socket are also stored here (in addition to participants_ map) so they're easier
             * to pass to poll when RTCP runner is listening to incoming packets */
            std::vector<uvgrtp::socket> sockets_;

            void (*sender_hook_)(uvgrtp::frame::rtcp_sender_report *);
            void (*receiver_hook_)(uvgrtp::frame::rtcp_receiver_report *);
            void (*sdes_hook_)(uvgrtp::frame::rtcp_sdes_packet *);
            void (*app_hook_)(uvgrtp::frame::rtcp_app_packet *);

            std::function<void(std::shared_ptr<uvgrtp::frame::rtcp_sender_report>)> sr_hook_f_;
            std::function<void(std::shared_ptr<uvgrtp::frame::rtcp_receiver_report>)> rr_hook_f_;
            std::function<void(std::shared_ptr<uvgrtp::frame::rtcp_sdes_packet>)> sdes_hook_f_;
            std::function<void(std::shared_ptr<uvgrtp::frame::rtcp_app_packet>)> app_hook_f_;
    };
};

namespace uvg_rtp = uvgrtp;
