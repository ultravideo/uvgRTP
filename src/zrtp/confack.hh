#pragma once

#include "defines.hh"
#include "zrtp_receiver.hh"
#include "zrtp_message.hh"

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

        class confack : public zrtp_message {
            public:
                confack(zrtp_session_t& session);
                ~confack();

                /* TODO:  */
                virtual rtp_error_t parse_msg(uvgrtp::zrtp_msg::receiver& receiver,
                    zrtp_session_t& session);
        };
    };
};

namespace uvg_rtp = uvgrtp;
