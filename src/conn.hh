#pragma once

#ifdef _WIN32
#include <inaddr.h>
#include <winsock2.h>
#else
#include <netinet/ip.h>
#endif

#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "frame.hh"
#include "socket.hh"
#include "util.hh"

namespace kvz_rtp {
    class connection {

    public:
        connection(bool reader);
        virtual ~connection();

        virtual rtp_error_t start() = 0;

        uint16_t  get_sequence() const;
        uint32_t  get_ssrc() const;
        uint8_t   get_payload() const;

        socket   get_socket() const;
        socket_t get_raw_socket() const;

        void set_payload(rtp_format_t fmt);
        void set_ssrc(uint32_t ssrc);

        /* functions for increasing various RTP statistics
         *
         * overloaded functions without parameters increase the counter by 1*/
        void inc_rtp_sequence(size_t n);
        void inc_processed_bytes(size_t n);
        void inc_overhead_bytes(size_t n);
        void inc_total_bytes(size_t n);
        void inc_processed_pkts(size_t n);

        void inc_processed_pkts();
        void inc_rtp_sequence();

        /* config set and get */
        void set_config(void *config);
        void *get_config();

        /* helper function fill the rtp header to allocated buffer,
         * caller must make sure that the buffer is at least 12 bytes long */
        void fill_rtp_header(uint8_t *buffer, uint32_t timestamp);

    protected:
        void *config_;
        uint32_t id_;

        kvz_rtp::socket socket_;

    private:
        bool reader_;

        /* RTP */
        uint16_t rtp_sequence_;
        uint8_t  rtp_payload_;
        uint32_t rtp_ssrc_;

        /* statistics for RTCP reports */
        size_t processed_bytes_;
        size_t overhead_bytes_;
        size_t total_bytes_;
        size_t processed_pkts_;
        size_t dropped_pkts_;
    };
};
