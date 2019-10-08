#pragma once

#include <thread>

#include "conn.hh"
#include "frame.hh"
#include "socket.hh"

namespace kvz_rtp {
    class reader : public connection {

    public:
        reader(rtp_format_t fmt, std::string src_addr, int src_port);
        ~reader();

        /* NOTE: this operation is blocking */
        kvz_rtp::frame::rtp_frame *pull_frame();

        // open socket and start runner_
        rtp_error_t start();

        bool active();

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
        kvz_rtp::frame::rtp_frame *validate_rtp_frame(uint8_t *buffer, int size);

        /* Helper function for returning received RTP frames to user (just to make code look cleaner) */
        void return_frame(kvz_rtp::frame::rtp_frame *frame);

    private:
        // TODO implement ring buffer
        bool active_;

        // connection-related stuff
        std::string src_addr_;
        int src_port_;

        // receiver thread related stuff
        std::thread *runner_;
        uint8_t *recv_buffer_;     /* buffer for incoming packet (MAX_PACKET) */
        uint32_t recv_buffer_len_; /* buffer length */

        std::vector<kvz_rtp::frame::rtp_frame *>  framesOut_;
        std::mutex frames_mtx_;

        // TODO
        void *recv_hook_arg_;
        void (*recv_hook_)(void *arg, kvz_rtp::frame::rtp_frame *frame);
    };
};
