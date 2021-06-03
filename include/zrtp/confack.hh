#pragma once

#include "zrtp/defines.hh"
#include "zrtp/zrtp_receiver.hh"

#include "util.hh"

#ifndef _WIN32
#include <netinet/in.h>
#endif

namespace uvgrtp {

    namespace frame {
        struct zrtp_frame;
    };

    typedef struct zrtp_session zrtp_session_t;

    namespace zrtp_msg {

        PACK(struct zrtp_confack {
            zrtp_msg msg_start;
            uint32_t crc = 0;
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
