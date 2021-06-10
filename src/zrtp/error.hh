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

        PACK(struct zrtp_error {
            zrtp_msg msg_start;
            uint32_t error = 0;
            uint32_t crc = 0;
        });

        class error : public zrtp_message {
            public:
                error(int error_code);
                ~error();

                virtual rtp_error_t parse_msg(uvgrtp::zrtp_msg::receiver& receiver,
                                              zrtp_session_t& session);
        };

    };
};

namespace uvg_rtp = uvgrtp;
