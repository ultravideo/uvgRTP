#pragma once

#include "frame.hh"
#include "socket.hh"
#include "util.hh"
#include "zrtp/defines.hh"
#include "zrtp/zrtp_receiver.hh"

namespace uvg_rtp {

    namespace zrtp_msg {

        PACK(struct zrtp_hello_ack {
            zrtp_msg msg_start;
            uint32_t crc;
        });

        class hello_ack {
            public:
                hello_ack();
                ~hello_ack();

                rtp_error_t send_msg(uvg_rtp::socket *socket, sockaddr_in& addr);

                rtp_error_t parse_msg(uvg_rtp::zrtp_msg::receiver& receiver);

            private:
                uvg_rtp::frame::zrtp_frame *frame_;
                size_t len_;
        };

    };
};
