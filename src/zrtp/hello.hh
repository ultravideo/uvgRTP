#pragma once

#include "defines.hh"
#include "zrtp_message.hh"

#include "util.hh"

#ifndef _WIN32
#include <netinet/in.h>
#endif

namespace uvgrtp {

    typedef struct capabilities zrtp_capab_t;
    typedef struct zrtp_session zrtp_session_t;

    class socket;

    namespace frame {
        struct zrtp_frame;
    };

    namespace zrtp_msg {

        class receiver;

        PACK(struct zrtp_hello {
            zrtp_msg msg_start;

            uint32_t version = 0;
            uint32_t client[4];
            uint32_t hash[8];
            uint32_t zid[3];

            uint8_t zero:1;
            uint8_t s:1;
            uint8_t m:1;
            uint8_t p:1;
            uint8_t unused = 0;
            uint8_t hc:4;
            uint8_t cc:4;
            uint8_t ac:4;
            uint8_t kc:4;
            uint8_t sc:4;

            uint64_t mac = 0;
            uint32_t crc = 0;
        });

        class hello : public zrtp_message {
            public:
                hello(zrtp_session_t& session);
                ~hello();

                /* TODO:  */
                virtual rtp_error_t parse_msg(uvgrtp::zrtp_msg::receiver& receiver, zrtp_session_t& session);
        };
    };
};

namespace uvg_rtp = uvgrtp;
