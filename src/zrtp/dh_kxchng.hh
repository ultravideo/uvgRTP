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

        PACK(struct zrtp_dh {
            zrtp_msg msg_start;
            uint32_t hash[8];
            uint8_t rs1_id[8];
            uint8_t rs2_id[8];
            uint8_t aux_secret[8];
            uint8_t pbx_secret[8];
            uint8_t pk[384];
            uint8_t mac[8];
            uint32_t crc = 0;
        });

        class dh_key_exchange : public zrtp_message {
            public:
                dh_key_exchange(zrtp_session_t& session, int part);
                dh_key_exchange(struct zrtp_dh *dh);
                ~dh_key_exchange();

                /* TODO:  */
                virtual rtp_error_t parse_msg(uvgrtp::zrtp_msg::receiver& receiver, zrtp_session_t& session);

        };
    };
};

namespace uvg_rtp = uvgrtp;
