#pragma once

#include "frame.hh"
#include "socket.hh"
#include "util.hh"

#include "zrtp/defines.hh"
#include "zrtp/zrtp_receiver.hh"

namespace uvg_rtp {

    namespace zrtp_msg {

        PACKED_STRUCT(zrtp_confack) {
            zrtp_msg msg_start;
            uint32_t crc;
        };

        class confack {
            public:
                confack(zrtp_session_t& session);
                ~confack();

                /* TODO:  */
                rtp_error_t send_msg(uvg_rtp::socket *socket, sockaddr_in& addr);

                /* TODO:  */
                rtp_error_t parse_msg(uvg_rtp::zrtp_msg::receiver& receiver);

            private:
                uvg_rtp::frame::zrtp_frame *frame_;
                uvg_rtp::frame::zrtp_frame *rframe_;
                size_t len_, rlen_;
        };
    };
};
