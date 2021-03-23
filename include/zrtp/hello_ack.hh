#pragma once

#include "frame.hh"
#include "socket.hh"
#include "util.hh"
#include "zrtp/defines.hh"
#include "zrtp/zrtp_receiver.hh"

namespace uvgrtp {

    namespace zrtp_msg {

        PACK(struct zrtp_hello_ack {
            zrtp_msg msg_start;
            uint32_t crc;
        });

        class hello_ack {
            public:
                hello_ack();
                ~hello_ack();

                rtp_error_t send_msg(uvgrtp::socket *socket, sockaddr_in& addr);

                rtp_error_t parse_msg(uvgrtp::zrtp_msg::receiver& receiver);

            private:
                uvgrtp::frame::zrtp_frame *frame_;
                size_t len_;
        };

    };
};

namespace uvg_rtp = uvgrtp;
