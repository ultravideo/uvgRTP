#pragma once

#include "zrtp/defines.hh"
#include "zrtp/zrtp_receiver.hh"

#include "frame.hh"
#include "socket.hh"
#include "util.hh"


namespace uvgrtp {

    namespace zrtp_msg {

        PACK(struct zrtp_error {
            zrtp_msg msg_start;
            uint32_t error;
            uint32_t crc;
        });

        class error {
            public:
                error(int error_code);
                ~error();

                rtp_error_t send_msg(uvgrtp::socket *socket, sockaddr_in& addr);

                rtp_error_t parse_msg(uvgrtp::zrtp_msg::receiver& receiver);

            private:
                uvgrtp::frame::zrtp_frame *frame_;
                size_t len_;
        };

    };
};

namespace uvg_rtp = uvgrtp;
