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
#ifdef _WIN32
        SOCKET    get_socket() const;
#else
        int       get_socket() const;
#endif

        void set_payload(rtp_format_t fmt);
        void set_ssrc(uint32_t ssrc);

        /* TODO: this is awful! */
        void incRTPSequence(uint16_t seq);
        void incProcessedBytes(uint32_t nbytes);
        void incOverheadBytes(uint32_t nbytes);
        void incTotalBytes(uint32_t nbytes);
        void incProcessedPackets(uint32_t npackets);

        /* config set and get */
        void set_config(void *config);
        void *get_config();

        /* helper function fill the rtp header to allocated buffer,
         * caller must make sure that the buffer is at least 12 bytes long */
        void fill_rtp_header(uint8_t *buffer, uint32_t timestamp);

    protected:
        void *config_;
        uint32_t id_;
#ifdef _WIN32
        SOCKET socket_;
#else
        int socket_;
#endif

    private:
        /* TODO: should these be public so we could get rid of setters/getters */
        bool reader_;

        // RTP
        uint16_t rtp_sequence_;
        uint8_t  rtp_payload_;
        uint32_t rtp_ssrc_;

        // Statistics
        uint32_t processedBytes_;
        uint32_t overheadBytes_;
        uint32_t totalBytes_;
        uint32_t processedPackets_;
    };
};
