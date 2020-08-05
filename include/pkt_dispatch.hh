#pragma once

#include <mutex>
#include <unordered_map>

#include "frame.hh"
#include "runner.hh"
#include "socket.hh"
#include "util.hh"

namespace uvg_rtp {

    typedef rtp_error_t (*packet_handler)(ssize_t, void *, int, uvg_rtp::frame::rtp_frame **);

    class pkt_dispatcher : public runner {
        public:
            pkt_dispatcher(uvg_rtp::socket socket);
            ~pkt_dispatcher();

            /* Install a generic handler for an incoming packet
             *
             * Return RTP_OK on successfully
             * Return RTP_INVALID_VALUE if "handler" is nullptr */
            rtp_error_t install_handler(packet_handler handler);

            /* Install receive hook for the RTP packet dispatcher
             *
             * Return RTP_OK on success
             * Return RTP_INVALID_VALUE if "hook" is nullptr */
            rtp_error_t install_receive_hook(void *arg, void (*hook)(void *, uvg_rtp::frame::rtp_frame *));

            /* Start the RTP packet dispatcher
             *
             * Return RTP_OK on success
             * Return RTP_MEMORY_ERROR if allocation of a thread object fails */
            rtp_error_t start(uvg_rtp::socket *socket, int flags);

            /* Fetchj frame from the frame queue that contains all received frame.
             * pull_frame() will block until there is a frame that can be returned.
             * If "timeout" is given, pull_frame() will block only for however long
             * that value tells it to.
             * If no frame is received within that time period, pull_frame() returns nullptr
             *
             * Return pointer to RTP frame on success
             * Return nullptr if operation timed out or an error occurred */
            uvg_rtp::frame::rtp_frame *pull_frame();
            uvg_rtp::frame::rtp_frame *pull_frame(size_t ms);

            /* Return reference to the vector that holds all installed handlers */
            std::vector<uvg_rtp::packet_handler>& get_handlers();

            /* RTP packet dispatcher thread */
            static void runner(uvg_rtp::pkt_dispatcher *dispatcher, uvg_rtp::socket *socket, int flags);

        private:

            uvg_rtp::socket socket_;
            std::vector<packet_handler> packet_handlers_;

            /* If receive hook has not been installed, frames are pushed to "frames_"
             * and they can be retrieved using pull_frame() */
            std::vector<uvg_rtp::frame::rtp_frame *> frames_;
            std::mutex frames_mtx_;

            void *recv_hook_arg_;
            void (*recv_hook_)(void *arg, uvg_rtp::frame::rtp_frame *frame);
    };
}
