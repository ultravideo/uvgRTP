#pragma once

#include "rtp.hh"
#include "runner.hh"
#include "socket.hh"

namespace kvz_rtp {

    class receiver : public runner {
        public:
            receiver(kvz_rtp::socket& socket, rtp_ctx_conf& conf, rtp_format_t fmt, kvz_rtp::rtp *rtp);
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
            kvz_rtp::frame::rtp_frame *pull_frame();

            /* Open socket, start frame receiver and RTCP
             *
             * Return RTP_OK on success
             * Return RTP_MEMORY_ERROR if memory deallocation failed
             * Return RTP_GENERIC_ERROR for any other error */
            rtp_error_t start();

            bool recv_hook_installed();
            void recv_hook(kvz_rtp::frame::rtp_frame *frame);
            void install_recv_hook(void *arg, void (*hook)(void *arg, kvz_rtp::frame::rtp_frame *));

            uint8_t *get_recv_buffer() const;
            uint32_t get_recv_buffer_len() const;

            void add_outgoing_frame(kvz_rtp::frame::rtp_frame *frame);

            /* Read RTP header field from "src" to "dst" changing the byte order from network to host where needed */
            rtp_error_t read_rtp_header(kvz_rtp::frame::rtp_header *dst, uint8_t *src);

            /* When a frame is received, this validate_rtp_frame() is called to validate the frame
             * and to construct the actual header and frame "buffer"
             *
             * Return valid RTP frame on success
             * Return nullptr if the frame is invalid */
            /* TODO: move to rtp.cc */
            kvz_rtp::frame::rtp_frame *validate_rtp_frame(uint8_t *buffer, int size);

            /* Helper function for returning received RTP frames to user (just to make code look cleaner) */
            void return_frame(kvz_rtp::frame::rtp_frame *frame);

            /* TODO:  */
            kvz_rtp::socket& get_socket();

            /* TODO:  */
            kvz_rtp::rtp *get_rtp_ctx();

        private:
            kvz_rtp::socket socket_;
            kvz_rtp::rtp *rtp_;
            rtp_ctx_conf conf_;
            rtp_format_t fmt_;

            uint8_t *recv_buf_;
            size_t   recv_buf_len_;

            /* Received frames are pushed here and they can fetched using pull_frame() */
            std::vector<kvz_rtp::frame::rtp_frame *> frames_;
            std::mutex frames_mtx_;

            /* An an alternative to pull_frame(), user can install
             * a receive hook which is called every time a frame is received */
            void *recv_hook_arg_;
            void (*recv_hook_)(void *arg, kvz_rtp::frame::rtp_frame *frame);
    };
};
