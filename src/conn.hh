#pragma once

#ifdef _WIN32
#include <winsock2.h>
#include <inaddr.h>
#else
#include <netinet/ip.h>
#endif

#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "frame.hh"
#include "rtcp.hh"
#include "socket.hh"
#include "util.hh"

namespace kvz_rtp {

    class frame_queue;

    class connection {
    public:
        connection(rtp_format_t fmt, bool reader);
        virtual ~connection();

        virtual rtp_error_t start() = 0;

        uint16_t  get_sequence() const;
        uint32_t  get_ssrc() const;
        uint8_t   get_payload() const;

        socket&  get_socket();
        socket_t get_raw_socket();

        void set_payload(rtp_format_t fmt);
        void set_ssrc(uint32_t ssrc);

        /* Functions for increasing various RTP statistics
         * Overloaded functions without parameters increase the counter by 1
         *
         * Functions that take SSRC are for updating receiver statistics
         *
         * TODO: jitter! */
        void inc_rtp_sequence(size_t n);
        void inc_sent_bytes(size_t n);
        void inc_sent_pkts(size_t n);
        void inc_sent_pkts();
        void inc_rtp_sequence();

        /* See RTCP->update_receiver_stats() for documentation */
        rtp_error_t update_receiver_stats(kvz_rtp::frame::rtp_frame *frame);

        /* config set and get */
        void set_config(void *config);
        void *get_config();

        /* helper function fill the rtp header to allocated buffer,
         * caller must make sure that the buffer is at least 12 bytes long */
        void fill_rtp_header(uint8_t *buffer);

        void update_rtp_sequence(uint8_t *buffer);

        /* Set clock rate for RTP timestamp in Hz
         * This must be set, otherwise the timestamps won't be correct */
        void set_clock_rate(uint32_t clock_rate);

        /* Create RTCP instance for this connection
         *
         * This instance listens to src_port for incoming RTCP reports and sends
         * repots about this session to dst_addr:dst_port every N seconds (see RFC 3550) */
        rtp_error_t create_rtcp(std::string dst_addr, int dst_port, int src_port);

        rtp_error_t add_rtcp_participant(kvz_rtp::connection *conn);

        /* TODO:  */
        kvz_rtp::frame_queue *get_frame_queue();

    protected:
        void *config_;
        uint32_t id_;

        kvz_rtp::socket socket_;
        kvz_rtp::rtcp *rtcp_;

    private:
        bool reader_;

        /* RTP */
        uint16_t rtp_sequence_;
        uint8_t  rtp_payload_;
        uint32_t rtp_ssrc_;
        uint32_t rtp_timestamp_;
        uint64_t wc_start_;
        kvz_rtp::clock::hrc::hrc_t wc_start_2;
        uint32_t clock_rate_;

        kvz_rtp::frame_queue *fqueue_;
    };
};
