#pragma once

#include <stdint.h>

#include "conn.hh"
#include "frame.hh"

namespace kvz_rtp {
    class writer : public connection {

    public:
        writer(std::string dst_addr, int dst_port);
        writer(std::string dst_addr, int dst_port, int src_port);
        ~writer();

        // open socket for sending frames
        rtp_error_t start();

        /* TODO:  */
        rtp_error_t push_frame(uint8_t *data, uint32_t data_len, rtp_format_t fmt, uint32_t timestamp);

        /* TODO:  */
        rtp_error_t push_frame(kvz_rtp::frame::rtp_frame *frame, uint32_t timestamp);

        sockaddr_in get_out_address();

    private:
        std::string dst_addr_;
        int dst_port_;
        int src_port_;
        sockaddr_in addr_out_;
    };
};
