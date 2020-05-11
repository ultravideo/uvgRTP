#pragma once

#include <mutex>

#include "frame.hh"
#include "rtp.hh"
#include "runner.hh"
#include "socket.hh"

namespace uvg_rtp {

    class receiver : public runner {
        public:
            receiver(uvg_rtp::socket& socket, rtp_ctx_conf& conf, rtp_format_t fmt, uvg_rtp::rtp *rtp);
            ~receiver();


            /*
             * TODO
             * TODO
             * TODO       CLEAN ALL THIS CODE!!!!
             * TODO
             * TODO
             * TODO
             *
             */

            /* NOTE: this operation is blocking */
            uvg_rtp::frame::rtp_frame *pull_frame();

            /* Block at most "timeout" milliseconds and return nullptr if nothing was received */
            uvg_rtp::frame::rtp_frame *pull_frame(size_t timeout);

            /* Open socket, start frame receiver and RTCP
             *
             * Return RTP_OK on success
             * Return RTP_MEMORY_ERROR if memory deallocation failed
             * Return RTP_GENERIC_ERROR for any other error */
            rtp_error_t start();

            rtp_error_t stop();

            bool recv_hook_installed();
            void recv_hook(uvg_rtp::frame::rtp_frame *frame);
            void install_recv_hook(void *arg, void (*hook)(void *arg, uvg_rtp::frame::rtp_frame *));
            void install_notify_hook(void *arg, void (*hook)(void *arg, int notify));

            uint8_t *get_recv_buffer() const;
            uint32_t get_recv_buffer_len() const;

            void add_outgoing_frame(uvg_rtp::frame::rtp_frame *frame);

            /* Read RTP header field from "src" to "dst" changing the byte order from network to host where needed */
            rtp_error_t read_rtp_header(uvg_rtp::frame::rtp_header *dst, uint8_t *src);

            /* When a frame is received, this validate_rtp_frame() is called to validate the frame
             * and to construct the actual header and frame "buffer"
             *
             * Return valid RTP frame on success
             * Return nullptr if the frame is invalid */
            /* TODO: move to rtp.cc */
            uvg_rtp::frame::rtp_frame *validate_rtp_frame(uint8_t *buffer, int size);

            /* Helper function for returning received RTP frames to user (just to make code look cleaner) */
            void return_frame(uvg_rtp::frame::rtp_frame *frame);

            /* TODO:  */
            uvg_rtp::socket& get_socket();

            /* TODO:  */
            uvg_rtp::rtp *get_rtp_ctx();

            /* TODO:  */
            std::mutex& get_mutex();

            /* Get reference to the media stream's config structure */
            rtp_ctx_conf& get_conf();

        private:
            uvg_rtp::socket socket_;
            uvg_rtp::rtp *rtp_;
            rtp_ctx_conf conf_;
            rtp_format_t fmt_;

            uint8_t *recv_buf_;
            size_t   recv_buf_len_;

            /* Received frames are pushed here and they can fetched using pull_frame() */
            std::vector<uvg_rtp::frame::rtp_frame *> frames_;
            std::mutex frames_mtx_;
            std::mutex r_mtx_;

            /* An an alternative to pull_frame(), user can install
             * a receive hook which is called every time a frame is received */
            void *recv_hook_arg_;
            void (*recv_hook_)(void *arg, uvg_rtp::frame::rtp_frame *frame);

            /* If user so wishes, he may install a notify hook that is used
             * by the frame receiver to inform, for example, that a frame is late or lost */
            void *notify_hook_arg_;
            void (*notify_hook_)(void *arg, int notify);
    };
};
