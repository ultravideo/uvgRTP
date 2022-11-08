#pragma once

#include "clock.hh"
#include "util.hh"
#include "frame.hh"

#include <bitset>
#include <map>
#include <thread>
#include <vector>
#include <functional>
#include <memory>
#include <mutex>
#include <deque>

namespace uvgrtp {

    class rtp;
    class srtcp;
    class socket;

    typedef std::vector<std::pair<size_t, uint8_t*>> buf_vec; // also defined in socket.hh

    /// \cond DO_NOT_DOCUMENT
    enum RTCP_ROLE {
        RECEIVER,
        SENDER
    };

    struct sender_statistics {
        /* sender stats */
        uint32_t sent_pkts = 0;      /* Number of sent RTP packets */
        uint32_t sent_bytes = 0;     /* Number of sent bytes excluding RTP Header */
        bool sent_rtp_packet = false; // since last report
    };

    struct receiver_statistics {
        /* receiver stats */
        uint32_t received_pkts = 0;  /* Number of packets received */
        uint32_t dropped_pkts = 0;   /* Number of dropped RTP packets */
        uint32_t received_bytes = 0; /* Number of bytes received excluding RTP Header */
        bool received_rtp_packet = false; // since last report

        double jitter = 0;            /* The estimation of jitter (see RFC 3550 A.8) */
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
        uint16_t cycles = 0;         /* Number of sequence cycles */
    };

    struct rtcp_participant {
        std::shared_ptr<uvgrtp::socket> socket = nullptr; /* socket associated with this participant */
        sockaddr_in address = {};                              /* address of the participant */
        struct receiver_statistics stats;                 /* RTCP session statistics of the participant */

        uint32_t probation = 0;                           /* has the participant been fully accepted to the session */
        int role = 0;                                     /* is the participant a sender or a receiver */

        /* Save the latest RTCP packets received from this participant
         * Users can query these packets using the SSRC of participant */
        uvgrtp::frame::rtcp_sender_report   *sr_frame = nullptr;
        uvgrtp::frame::rtcp_receiver_report *rr_frame = nullptr;
        uvgrtp::frame::rtcp_sdes_packet     *sdes_frame = nullptr;
        uvgrtp::frame::rtcp_app_packet      *app_frame = nullptr;
    };

    struct rtcp_app_packet {
        rtcp_app_packet(const rtcp_app_packet& orig_packet) = delete;
        rtcp_app_packet(const char* name, uint8_t subtype, uint32_t payload_len, const uint8_t* payload);
        ~rtcp_app_packet();

        const char* name;
        uint8_t subtype;

        uint32_t payload_len;
        const uint8_t* payload;
    };
    /// \endcond

    /**
     * \brief RTCP instance handles all incoming and outgoing RTCP traffic, including report generation
     *
     * \details If media_stream was created with RCE_RTCP flag, RTCP is enabled. RTCP periodically sends compound RTCP packets. 
     * The bit rate of RTP session influences the reporting interval, but changing this has not yet been implemented.
     *
     * The compound RTCP packet begins with either Sender Reports if we sent RTP packets recently or Receiver Report if we didn't 
     * send RTP packets recently. Both of these report types include report blocks for all the RTP sources we have received packets 
     * from during reporting period. The compound packets also always have an SDES packet and calling send_sdes_packet()-function will 
     * modify the contents of this SDES packet.
     *
     * You can use the APP packet to test new RTCP packet types using the send_app_packet()-function. 
     * The APP packets are added to these periodically sent compound packets.
     * 
     * 
     * See <a href="https://www.rfc-editor.org/rfc/rfc3550#section-6" target="_blank">RFC 3550 section 6</a> for more details. 
     */
    class rtcp {
        public:
            /// \cond DO_NOT_DOCUMENT
            rtcp(std::shared_ptr<uvgrtp::rtp> rtp, std::string cname, int rce_flags);
            rtcp(std::shared_ptr<uvgrtp::rtp> rtp, std::string cname, std::shared_ptr<uvgrtp::srtcp> srtcp, int rce_flags);
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
            rtp_error_t send_sdes_packet(const std::vector<uvgrtp::frame::rtcp_sdes_item>& items);

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
            std::vector<std::shared_ptr<uvgrtp::socket>>& get_sockets();

            /* Somebody joined the multicast group the owner of this RTCP instance is part of
             * Add it to RTCP participant list so we can start listening for reports
             *
             * "clock_rate" tells how much the RTP timestamp advances, this information is needed
             * to calculate the interarrival jitter correctly. It has nothing do with our clock rate,
             * (or whether we're even sending anything)
             *
             * Return RTP_OK on success and RTP_ERROR on error */
            rtp_error_t add_participant(std::string src_addr, std::string dst_addr, uint16_t dst_port, uint16_t src_port, uint32_t clock_rate);

            /* Functions for updating various RTP sender statistics */
            void sender_update_stats(const uvgrtp::frame::rtp_frame *frame);

            /* If we've detected that our SSRC has collided with someone else's SSRC, we need to
             * generate new random SSRC and reinitialize our own RTCP state.
             * RTCP object still has the participants of "last session", we can use their SSRCs
             * to detected new collision
             *
             * Return RTP_OK if reinitialization succeeded
             * Return RTP_SSRC_COLLISION if our new SSRC has collided and we need to generate new SSRC */
            rtp_error_t reset_rtcp_state(uint32_t ssrc);

            /* Update various session statistics */
            void update_session_statistics(const uvgrtp::frame::rtp_frame *frame);

            /* Getter for interval_ms_, which is calculated by set_session_bandwidth */
            uint32_t get_rtcp_interval_ms() const;

            void set_session_bandwidth(uint32_t kbps);

            /* Return SSRCs of all participants */
            std::vector<uint32_t> get_participants() const;
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

            /**
             * \brief Install an RTCP Sender Report hook
             *
             * \details This function is called when an RTCP Sender Report is received
             *
             * \param sr_handler C++ function pointer to the hook
             *
             * \retval RTP_OK on success
             * \retval RTP_INVALID_VALUE If hook is nullptr
             */
            rtp_error_t install_sender_hook(std::function<void(std::unique_ptr<uvgrtp::frame::rtcp_sender_report>)> sr_handler);

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

            /**
             * \brief Install an RTCP Receiver Report hook
             *
             * \details This function is called when an RTCP Receiver Report is received
             *
             * \param rr_handler C++ function pointer to the hook
             *
             * \retval RTP_OK on success
             * \retval RTP_INVALID_VALUE If hook is nullptr
             */
            rtp_error_t install_receiver_hook(std::function<void(std::unique_ptr<uvgrtp::frame::rtcp_receiver_report>)> rr_handler);

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

            /**
             * \brief Install an RTCP SDES packet hook
             *
             * \details This function is called when an RTCP SDES packet is received
             *
             * \param sdes_handler C++ function pointer to the hook
             *
             * \retval RTP_OK on success
             * \retval RTP_INVALID_VALUE If hook is nullptr
             */
            rtp_error_t install_sdes_hook(std::function<void(std::unique_ptr<uvgrtp::frame::rtcp_sdes_packet>)> sdes_handler);

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

            /**
             * \brief Install an RTCP APP packet hook
             *
             * \details This function is called when an RTCP APP packet is received
             *
             * \param app_handler C++ function pointer to the hook
             *
             * \retval RTP_OK on success
             * \retval RTP_INVALID_VALUE If hook is nullptr
             */
            rtp_error_t install_app_hook(std::function<void(std::unique_ptr<uvgrtp::frame::rtcp_app_packet>)> app_handler);

            /// \cond DO_NOT_DOCUMENT
            // These have been replaced by functions with unique_ptr in them
            rtp_error_t install_sender_hook(std::function<void(std::shared_ptr<uvgrtp::frame::rtcp_sender_report>)> sr_handler);
            rtp_error_t install_receiver_hook(std::function<void(std::shared_ptr<uvgrtp::frame::rtcp_receiver_report>)> rr_handler);
            rtp_error_t install_sdes_hook(std::function<void(std::shared_ptr<uvgrtp::frame::rtcp_sdes_packet>)> sdes_handler);
            rtp_error_t install_app_hook(std::function<void(std::shared_ptr<uvgrtp::frame::rtcp_app_packet>)> app_handler);
            /// \endcond
            rtp_error_t install_app_sending_hook(std::function<std::map<std::string, std::shared_ptr<rtcp_app_packet>>()> app_sending);

            /**
             * \brief Remove all installed hooks for RTCP
             *
             * \details Removes all installed hooks so they can be readded in case of changes
             *
             * \retval RTP_OK on success
             */
            rtp_error_t remove_all_hooks();

            /// \cond DO_NOT_DOCUMENT
            /* Update RTCP-related sender statistics */
            rtp_error_t update_sender_stats(size_t pkt_size);

            /* Update RTCP-related receiver statistics */
            static rtp_error_t recv_packet_handler(void *arg, int rce_flags, frame::rtp_frame **out);

            /* Update RTCP-related sender statistics */
            static rtp_error_t send_packet_handler_vec(void *arg, uvgrtp::buf_vec& buffers);

            // the length field is the rtcp packet size measured in 32-bit words - 1
            size_t rtcp_length_in_bytes(uint16_t length);

            void set_payload_size(size_t mtu_size);
            /// \endcond

        private:

            rtp_error_t set_sdes_items(const std::vector<uvgrtp::frame::rtcp_sdes_item>& items);

            uint32_t size_of_ready_app_packets(std::map<std::string, std::shared_ptr<rtcp_app_packet>> app_packets) const;

            uint32_t size_of_compound_packet(uint16_t reports,
                bool sr_packet, bool rr_packet, bool sdes_packet, uint32_t app_size, bool bye_packet) const;

            /* read the header values from rtcp packet */
            void read_rtcp_header(const uint8_t* buffer, size_t& read_ptr, 
                uvgrtp::frame::rtcp_header& header);
            void read_reports(const uint8_t* buffer, size_t& read_ptr, size_t packet_end, uint8_t count,
                std::vector<uvgrtp::frame::rtcp_report_block>& reports);

            void read_ssrc(const uint8_t* buffer, size_t& read_ptr, uint32_t& out_ssrc);

            /* Handle different kinds of incoming rtcp packets. The read header is passed to functions
               which read rest of the frame type specific data.
             * Return RTP_OK on success and RTP_ERROR on error */
            rtp_error_t handle_sender_report_packet(uint8_t* buffer, size_t& read_ptr, size_t packet_end,
                uvgrtp::frame::rtcp_header& header);
            rtp_error_t handle_receiver_report_packet(uint8_t* buffer, size_t& read_ptr, size_t packet_end,
                uvgrtp::frame::rtcp_header& header);
            rtp_error_t handle_sdes_packet(uint8_t* buffer, size_t& read_ptr, size_t packet_end,
                uvgrtp::frame::rtcp_header& header, uint32_t sender_ssrc);
            rtp_error_t handle_bye_packet(uint8_t* buffer, size_t& read_ptr, size_t packet_end,
                uvgrtp::frame::rtcp_header& header);
            rtp_error_t handle_app_packet(uint8_t* buffer, size_t& read_ptr, size_t packet_end,
                uvgrtp::frame::rtcp_header& header);

            static void rtcp_runner(rtcp *rtcp, int interval);

            /* when we start the RTCP instance, we don't know what the SSRC of the remote is
             * when an RTP packet is received, we must check if we've already received a packet
             * from this sender and if not, create new entry to receiver_stats_ map */
            bool is_participant(uint32_t ssrc) const;

            /* When we receive an RTP or RTCP packet, we need to check the source address and see if it's
             * the same address where we've received packets before.
             *
             * If the address is new, it means we have detected an SSRC collision and the paket should
             * be dropped We also need to check whether this SSRC matches with our own SSRC and if it does
             * we need to send RTCP BYE and rejoin to the session */
            bool collision_detected(uint32_t ssrc, const sockaddr_in& src_addr) const;

            /* Move participant from initial_peers_ to participants_ */
            rtp_error_t add_participant(uint32_t ssrc);

            /* We've got a message from new source (the SSRC of the frame is not known to us)
             * Initialize statistics for the peer and move it to participants_ */
            rtp_error_t init_new_participant(const uvgrtp::frame::rtp_frame *frame);

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
            void zero_stats(uvgrtp::sender_statistics *stats);

            void zero_stats(uvgrtp::receiver_statistics *stats);

            /* Takes ownership of the frame */
            rtp_error_t send_rtcp_packet_to_participants(uint8_t* frame, uint32_t frame_size, bool encrypt);

            void free_participant(std::unique_ptr<rtcp_participant> participant);

            void cleanup_participants();

            /* Secure RTCP context */
            std::shared_ptr<uvgrtp::srtcp> srtcp_;

            /* RTP context flags */
            int rce_flags_;

            /* are we a sender (and possible a receiver) or just a receiver */
            int our_role_;

            /* TODO: time_t?? */
            // TODO: Check these, they don't seem to be used
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
            // TODO: Not used anywhere at the moment
            size_t rtcp_bandwidth_;

            /* Flag that is true if the application has sent data since
             * the 2nd previous RTCP report was transmitted. */
            // TODO: Only set, never read
            bool we_sent_;

            /* The average compound RTCP packet size, in octets,
             * over all RTCP packets sent and received by this participant. The
             * size includes lower-layer transport and network protocol headers
             * (e.g., UDP and IP) as explained in Section 6.2 */
             // TODO: Only set, never read
            size_t avg_rtcp_pkt_pize_;

            /* Number of RTCP packets and bytes sent and received by this participant */
            // TODO: Only set, never read
            size_t rtcp_pkt_count_;
            size_t rtcp_byte_count_;

            /* Number of RTCP packets sent */
            uint32_t rtcp_pkt_sent_count_;

            /* Flag that is true if the application has not yet sent an RTCP packet. */
            // TODO: Only set, never read
            bool initial_;

            /* Copy of our own current SSRC */
            const uint32_t ssrc_;

            /* NTP timestamp associated with initial RTP timestamp (aka t = 0) */
            uint64_t clock_start_;

            /* Clock rate of the media ie. how fast does the time increase */
            uint32_t clock_rate_;

            /* The first value of RTP timestamp (aka t = 0) */
            uint32_t rtp_ts_start_;

            std::map<uint32_t, std::unique_ptr<rtcp_participant>> participants_;
            uint8_t num_receivers_; // maximum is 32 at the moment (5 bits)

            /* statistics for RTCP Sender and Receiver Reports */
            struct sender_statistics our_stats;

            /* If we expect frames from remote but haven't received anything from remote yet,
             * the participant resides in this vector until he's moved to participants_ */
            std::vector<std::unique_ptr<rtcp_participant>> initial_participants_;

            /* Vector of sockets the RTCP runner is listening to
             *
             * The socket are also stored here (in addition to participants_ map) so they're easier
             * to pass to poll when RTCP runner is listening to incoming packets */
            std::vector<std::shared_ptr<uvgrtp::socket>> sockets_;

            void (*sender_hook_)(uvgrtp::frame::rtcp_sender_report *);
            void (*receiver_hook_)(uvgrtp::frame::rtcp_receiver_report *);
            void (*sdes_hook_)(uvgrtp::frame::rtcp_sdes_packet *);
            void (*app_hook_)(uvgrtp::frame::rtcp_app_packet *);

            std::function<void(std::shared_ptr<uvgrtp::frame::rtcp_sender_report>)>   sr_hook_f_;
            std::function<void(std::unique_ptr<uvgrtp::frame::rtcp_sender_report>)>   sr_hook_u_;
            std::function<void(std::shared_ptr<uvgrtp::frame::rtcp_receiver_report>)> rr_hook_f_;
            std::function<void(std::unique_ptr<uvgrtp::frame::rtcp_receiver_report>)> rr_hook_u_;
            std::function<void(std::shared_ptr<uvgrtp::frame::rtcp_sdes_packet>)>     sdes_hook_f_;
            std::function<void(std::unique_ptr<uvgrtp::frame::rtcp_sdes_packet>)>     sdes_hook_u_;
            std::function<void(std::shared_ptr<uvgrtp::frame::rtcp_app_packet>)>      app_hook_f_;
            std::function<void(std::unique_ptr<uvgrtp::frame::rtcp_app_packet>)>      app_hook_u_;

            std::function<std::map<std::string, std::shared_ptr<rtcp_app_packet>>()> app_sending_hook_;

            std::mutex sr_mutex_;
            std::mutex rr_mutex_;
            std::mutex sdes_mutex_;
            std::mutex app_mutex_;

            std::unique_ptr<std::thread> report_generator_;

            bool is_active() const
            {
                return active_;
            }

            bool active_;

            uint32_t interval_ms_;

            std::mutex packet_mutex_;

            // messages waiting to be sent
            std::vector<uvgrtp::frame::rtcp_sdes_item> ourItems_; // always sent
            std::vector<uint32_t> bye_ssrcs_; // sent once

            uvgrtp::frame::rtcp_sdes_item cnameItem_;
            char cname_[255];

            size_t mtu_size_;
    };
}

namespace uvg_rtp = uvgrtp;
