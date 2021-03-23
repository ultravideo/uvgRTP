#pragma once

#include "frame.hh"
#include "socket.hh"
#include "util.hh"

#include "zrtp/defines.hh"
#include "zrtp/zrtp_receiver.hh"

namespace uvgrtp {

    namespace zrtp_msg {

        PACK(struct zrtp_confack {
            zrtp_msg msg_start;
            uint32_t crc;
        });

        class confack {
            public:
                confack(zrtp_session_t& session);
                ~confack();

                /* TODO:  */
                rtp_error_t send_msg(uvgrtp::socket *socket, sockaddr_in& addr);

                /* TODO:  */
                rtp_error_t parse_msg(uvgrtp::zrtp_msg::receiver& receiver);

            private:
                uvgrtp::frame::zrtp_frame *frame_;
                uvgrtp::frame::zrtp_frame *rframe_;
                size_t len_, rlen_;
        };
    };
};

namespace uvg_rtp = uvgrtp;
