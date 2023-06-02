#pragma once

#include "uvgrtp/clock.hh"
#include "uvgrtp/util.hh"
#include "uvgrtp/frame.hh"

#ifdef _WIN32
#include <ws2ipdef.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#endif

#include <vector>
#include <memory>
#include <map>
#include <thread>
#include <functional>
#include <mutex>


namespace uvgrtp {
    class socketfactory;
    class rtcp;
    class socket;

    /* Every RTCP socket will have an RTCP reader that receives packets and distributes them to the correct RTCP
     * objects. RTCP objects are mapped via REMOTE SSRCs, the SSRC that they will be receiving packets from.
     * If NO socket multiplexing is done, this will be 0 by default. If there IS socket multiplexing, this will be the 
     * remote SSRC of the media stream, set via the RCC_REMOTE_SSRC context flag.
     */
    class rtcp_reader {

        public: 
            rtcp_reader();
            ~rtcp_reader();

            /* Start the report reader thread
             *
             * Return RTP_OK on success */

            rtp_error_t start();


            /* Stop the report reader thread
             *
             * Return RTP_OK on success */
            rtp_error_t stop();

            /* Set the RTCP readers socket
             *
             * Return true on success */
            rtp_error_t set_socket(std::shared_ptr<uvgrtp::socket> socket);

            /* Map a new RTCP object into a remote SSRC
             *
             * Param ssrc SSRC of the REMOTE stream that the given RTCP will receive from
             * Param rtcp RTCP object
             * Return RTP_OK on success */
            rtp_error_t map_ssrc_to_rtcp(std::shared_ptr<std::atomic<uint32_t>> ssrc, std::shared_ptr<uvgrtp::rtcp> rtcp);

            /* Clear an RTCP object with the given REMOTE SSRC from the RTCP reader
             *
             * Param remote_ssrc REMOTE SSRC of the RTCP that will be removed from the reader
             * Return 0 if the RTCP object is removed
             * Return 1 if the RTCP object is removed AND the reader has no RTCP objects left -> which
             * means that the reader will be stopped and RTCP is free to clear the port */
            int clear_rtcp_from_reader(std::shared_ptr<std::atomic<std::uint32_t>> remote_ssrc);

        private:

            void rtcp_report_reader();

            bool active_;
            std::shared_ptr<uvgrtp::socket> socket_;
            std::map<std::shared_ptr<std::atomic<uint32_t>>, std::shared_ptr<uvgrtp::rtcp>> rtcps_map_;
            std::unique_ptr<std::thread> report_reader_;
            std::mutex map_mutex_;
    };


}