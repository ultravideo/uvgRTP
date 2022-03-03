#pragma once

#include "uvgrtp/util.hh"

#include <atomic>
#include <memory>
#include <thread>

namespace uvgrtp {

    class socket;

    class holepuncher {
        public:
            holepuncher(std::shared_ptr<uvgrtp::socket> socket);
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

            std::shared_ptr<uvgrtp::socket> socket_;
            std::atomic<uint64_t> last_dgram_sent_;

            bool active_;
            std::unique_ptr<std::thread> runner_;
    };
}

namespace uvg_rtp = uvgrtp;
