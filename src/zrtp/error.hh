#pragma once

#include "defines.hh"

#include "util.hh"

#ifndef _WIN32
#include <netinet/in.h>
#endif

namespace uvgrtp {

    class socket;

    namespace frame {
        struct zrtp_frame;
    };

    namespace zrtp_msg {
        
        class receiver;

        PACK(struct zrtp_error {
            zrtp_msg msg_start;
            uint32_t error = 0;
            uint32_t crc = 0;
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
