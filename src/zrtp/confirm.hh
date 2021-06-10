#pragma once


#include "defines.hh"
#include "zrtp_message.hh"

#include "util.hh"

#ifndef _WIN32
#include <netinet/in.h>
#endif

namespace uvgrtp {

    typedef struct zrtp_session zrtp_session_t;
    class socket;

    namespace frame {
        struct zrtp_frame;
    };

    namespace zrtp_msg {

        class receiver;


        PACK(struct zrtp_confirm {
            zrtp_msg msg_start;

            uint8_t confirm_mac[8];
            uint8_t cfb_iv[16];

            /* encrypted portion starts */
            uint8_t hash[32];

            uint32_t unused:15;
            uint32_t sig_len:9; /* signature length*/
            uint32_t zeros:4;
            uint32_t e:1;        /*  */
            uint32_t v:1;        /*  */
            uint32_t d:1;        /*  */
            uint32_t a:1;        /*  */

            uint32_t cache_expr = 0; /* cache expiration interval */
            /* encrypted portion ends */

            uint32_t crc = 0;
        });

        class confirm : public zrtp_message {
            public:
                confirm(zrtp_session_t& session, int part);
                ~confirm();

                /* TODO:  */
                virtual rtp_error_t parse_msg(uvgrtp::zrtp_msg::receiver& receiver, zrtp_session_t& session);
        };
    };
};

namespace uvg_rtp = uvgrtp;
