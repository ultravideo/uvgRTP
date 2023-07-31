#pragma once

#include "defines.hh"
#include "zrtp_message.hh"

#include "uvgrtp/util.hh"

#ifndef _WIN32
#include <netinet/in.h>
#endif

namespace uvgrtp {

    typedef struct capabilities zrtp_capab_t;
    typedef struct zrtp_session zrtp_session_t;

    class socket;

    namespace frame {
        struct zrtp_frame;
    }

    namespace zrtp_msg {

        class receiver;

        PACK(struct zrtp_hello {
            zrtp_msg msg_start;

            uint32_t version = 0;
            uint32_t client[4];
            uint32_t hash[8];
            uint32_t zid[3];

            uint32_t zero:1;
            uint32_t s:1;
            uint32_t m:1;
            uint32_t p:1;
            uint32_t unused:8;
            uint32_t hc:4;
            uint32_t cc:4;
            uint32_t ac:4;
            uint32_t kc:4;
            uint32_t sc:4;

            
            /* The following fields could be here if they were not 0:
            *  hash algorithms
            *  cipher algorithms
            *  auth tag types
            *  Key Agreement Types
            *  SAS Types
            */

            uint64_t mac = 0;
            uint32_t crc = 0;
        });

        class hello : public zrtp_message {
            public:
                hello(zrtp_session_t& session);
                ~hello();

                /* TODO:  */
                virtual rtp_error_t parse_msg(uvgrtp::zrtp_msg::zrtp_hello* hello, zrtp_session_t& session, size_t len);
        };
    }
}

namespace uvg_rtp = uvgrtp;
