#pragma once

#include "defines.hh"
#include "zrtp_message.hh"

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

        PACK(struct zrtp_hello_ack {
            zrtp_msg msg_start;
            uint32_t crc = 0;
        });

        class hello_ack : public zrtp_message {
            public:
                hello_ack();
                ~hello_ack();

                virtual rtp_error_t parse_msg(uvgrtp::zrtp_msg::receiver& receiver,
                    zrtp_session_t& session);
        };

    };
};

namespace uvg_rtp = uvgrtp;
