#pragma once

#include <atomic>

#include "runner.hh"
#include "socket.hh"
#include "util.hh"

namespace uvgrtp {

    class holepuncher : public runner {
        public:
            holepuncher(uvgrtp::socket *socket);
            ~holepuncher();

            /* Create new thread object and start the holepuncher
             *
             * Return RTP_OK on success
             * Return RTP_MEMORY_ERROR if allocation fails */
            rtp_error_t start();

            /* Stop the holepuncher */
            rtp_error_t stop();

            /* Notify the holepuncher that application has called push_frame()
             * and keepalive functionality is not needed for the following time period */
            void notify();

        private:
            void keepalive();

            uvgrtp::socket *socket_;
            std::atomic<uint64_t> last_dgram_sent_;
    };
};

namespace uvg_rtp = uvgrtp;
