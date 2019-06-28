#pragma once

#include <bitset>
#include <map>
#include <thread>
#include <vector>

#include "clock.hh"
#include "frame.hh"
#include "socket.hh"
#include "util.hh"

namespace kvz_rtp {

    class connection;

    /* TODO: explain these constants */
    const int RTP_SEQ_MOD    = 1 << 16;
    const int MIN_SEQUENTIAL = 2;
    const int MAX_DROPOUT    = 3000;
    const int MAX_MISORDER   = 100;
    const int MIN_TIMEOUT    = 5000;

    class rtcp {
    public:
        rtcp(uint32_t ssrc, bool receiver);
        ~rtcp();

        /* start the RTCP runner thread
         *
         * return RTP_OK on success and RTP_MEMORY_ERROR if the allocation fails */
        rtp_error_t start();

        /* End the RTCP session and send RTCP BYE to all participants
         *
         * return RTP_OK on success */
        rtp_error_t terminate();

        /* return true if the connection is still considered active
         * and RTCP transmissions should continue */
        bool active() const;
        
        /* return true if this RTCP instance belongs to an RTP receiver 
         * and a receiver report should be generated, otherwise sender report is generated */
        bool receiver() const;

        /* Generate either RTCP Sender or Receiver report and sent it to all participants 
         * Return RTP_OK on success and RTP_ERROR on error */
        rtp_error_t generate_report();

        /* Handle different kinds of incoming packets 
         *
         * These routines will convert the fields of "frame" from network to host byte order
         *
         * Currently nothing's done with valid packets, at some point an API for
         * querying these reports is implemented
         *
         * Return RTP_OK on success and RTP_ERROR on error */
        rtp_error_t handle_sender_report_packet(kvz_rtp::frame::rtcp_sender_frame *frame);
        rtp_error_t handle_receiver_report_packet(kvz_rtp::frame::rtcp_receiver_frame *frame);
        rtp_error_t handle_sdes_packet(kvz_rtp::frame::rtcp_sdes_frame *frame);
        rtp_error_t handle_bye_packet(kvz_rtp::frame::rtcp_bye_frame *frame);
        rtp_error_t handle_app_packet(kvz_rtp::frame::rtcp_app_frame *frame);

        /* Handle incoming RTCP packet (first make sure it's a valid RTCP packet)
         * This function will call one of the above functions internally
         *
         * Return RTP_OK on success and RTP_ERROR on error */
        rtp_error_t handle_incoming_packet(uint8_t *buffer, size_t size);

        /* Send "frame" to all participants
         *
         * These routines will convert all necessary fields to network byte order
         *
         * Return RTP_OK on success
         * Return RTP_INVALID_VALUE if "frame" is in some way invalid
         * Return RTP_SEND_ERROR if sending "frame" did not succeed (see socket.hh for details) */
        rtp_error_t send_sender_report_packet(kvz_rtp::frame::rtcp_sender_frame *frame);
        rtp_error_t send_receiver_report_packet(kvz_rtp::frame::rtcp_receiver_frame *frame);
        rtp_error_t send_sdes_packet(kvz_rtp::frame::rtcp_sdes_frame *frame);
        rtp_error_t send_bye_packet(kvz_rtp::frame::rtcp_bye_frame *frame);
        rtp_error_t send_app_packet(kvz_rtp::frame::rtcp_app_frame *frame);

        /* TODO:  */
        std::vector<kvz_rtp::socket>& get_sockets();

        /* Somebody joined the multicast group the owner of this RTCP instance is part of
         * Add it to RTCP participant list so we can start listening for reports 
         *
         * "clock_rate" tells how much the RTP timestamp advances, this information is needed
         * to calculate the interarrival jitter correctly. It has nothing do with our clock rate,
         * (or whether we're even sending anything)
         *
         * Return RTP_OK on success and RTP_ERROR on error */
        rtp_error_t add_participant(std::string dst_addr, int dst_port, int src_port, uint32_t clock_rate);

        /* Functions for updating various RTP sender statistics */
        void sender_inc_seq_cycle_count();
        void sender_inc_sent_pkts(size_t n);
        void sender_inc_sent_bytes(size_t n);
        void sender_update_stats(kvz_rtp::frame::rtp_frame *frame);

        void receiver_inc_sent_bytes(uint32_t sender_ssrc, size_t n);
        void receiver_inc_overhead_bytes(uint32_t sender_ssrc, size_t n);
        void receiver_inc_total_bytes(uint32_t sender_ssrc, size_t n);
        void receiver_inc_sent_pkts(uint32_t sender_ssrc, size_t n);
        void receiver_update_stats(kvz_rtp::frame::rtp_frame *frame);

        /* Set wallclock reading for t = 0 and random RTP timestamp from where the counting is started
         * + clock rate for calculating the correct increment */
        void set_sender_ts_info(uint64_t clock_start, uint32_t clock_rate, uint32_t rtp_ts_start);

    private:
        static void rtcp_runner(rtcp *rtcp);

        /* when we start the RTCP instance, we don't know what the SSRC of the remote is
         * when an RTP packet is received, we must check if we've already received a packet 
         * from this sender and if not, create new entry to receiver_stats_ map */
        bool is_participant(uint32_t ssrc);

        /* Move participant from initial_peers_ to participants_ */
        void add_participant(uint32_t ssrc);

        /* We've got a message from new source (the SSRC of the frame is not known to us)
         * Initialize statistics for the peer and move it to participants_ */
        void init_new_participant(kvz_rtp::frame::rtp_frame *frame);

        /* Initialize the RTP Sequence related stuff of peer
         * This function assumes that the peer already exists in the participants_ map */
        void init_participant_seq(uint32_t ssrc, uint16_t base_seq);

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

        /* Functions for generating different kinds of reports. 
         * These functions will both generate the report and send it 
         *
         * Return RTP_OK on success and RTP_ERROR on error */
        rtp_error_t generate_sender_report();
        rtp_error_t generate_receiver_report();

        /* Generate CNAME for participant using host and login names */
        std::string generate_cname();

        std::thread *runner_;
        std::string cname_;
        bool receiver_;

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

        /* Flag that is true if the application has not yet sent an RTCP packet. */
        bool initial_;

        /* Flag that is true if the connection is still considered open.
         * When clients decided to leave, he calls rtcp->terminate()
         * which stops the rtcp runner and RTCP BYE message to all participants */
        bool active_;

        /* Copy of our own current SSRC */
        uint32_t ssrc_;

        /* NTP timestamp associated with initial RTP timestamp (aka t = 0) */
        uint64_t clock_start_;

        /* Clock rate of the media ie. how fast does the time increase */
        uint32_t clock_rate_;

        /* The first value of RTP timestamp (aka t = 0) */
        uint32_t rtp_ts_start_;

        struct statistics {
            /* receiver stats */
            uint32_t received_pkts;  /* Number of packets received */
            uint32_t dropped_pkts;   /* Number of dropped RTP packets */
            uint32_t received_bytes; /* Number of bytes received excluding RTP Header */

            /* sender stats */
            uint32_t sent_pkts;   /* Number of sent RTP packets */
            uint32_t sent_bytes;  /* Number of sent bytes excluding RTP Header */

            uint32_t jitter;      /* TODO: */
            uint32_t transit;     /* TODO: */

            /* Receiver clock related stuff */
            uint64_t initial_ntp; /* Wallclock reading when the first RTP packet was received */
            uint32_t initial_rtp; /* RTP timestamp of the first RTP packet received */
            uint32_t clock_rate;  /* Rate of the clock (used for jitter calculations) */

            uint32_t lsr;               /* Middle 32 bits of the 64-bit NTP timestamp of previous SR*/
            kvz_rtp::clock::tp_t sr_ts; /* When the last SR was received (used to calculate delay) */

            uint16_t max_seq;  /* Highest sequence number received */
            uint16_t base_seq; /* First sequence number received */
            uint16_t bad_seq;  /* TODO:  */
            uint16_t cycles;   /* Number of sequence cycles */
        };

        struct participant {
            kvz_rtp::socket *socket; /* socket associated with this participant */
            sockaddr_in address;     /* address of the participant */
            struct statistics stats; /* RTCP session statistics of the participant */

            int probation;           /* TODO: */
            bool sender;             /* Sender will create report block for other sender only */
        };

        std::map<uint32_t, struct participant *> participants_;
        size_t num_receivers_;

        /* statistics for RTCP Sender and Receiver Reports */
        struct statistics sender_stats;

        /* TODO: */
        std::vector<struct participant *> initial_participants_;

        /* Vector of sockets the RTCP runner is listening to
         *
         * The socket are also stored here (in addition to participants_ map) so they're easier
         * to pass to poll when RTCP runner is listening to incoming packets */
        std::vector<kvz_rtp::socket> sockets_;
    };
};
