#pragma once

#include "defines.hh"
#include "zrtp_message.hh"

#include "util.hh"

#ifndef _WIN32
#include <netinet/in.h>
#endif


namespace uvgrtp {

    namespace frame {
        struct zrtp_frame;
    }

    class socket;

    typedef struct zrtp_session zrtp_session_t;

    namespace zrtp_msg {

        class receiver;


        /* DH Commit Message */
        PACK(struct zrtp_commit {
            zrtp_msg msg_start;

            uint32_t hash[8];
            uint32_t zid[3];
            uint32_t hash_algo = 0;
            uint32_t cipher_algo = 0;
            uint32_t auth_tag_type = 0;
            uint32_t key_agreement_type = 0;
            uint32_t sas_type = 0;

            uint32_t hvi[8];
            uint32_t mac[2];
            uint32_t crc = 0;
        });

        class commit : public zrtp_message {
            public:
                commit(zrtp_session_t& session);
                ~commit();

                /* TODO:  */
                virtual rtp_error_t parse_msg(uvgrtp::zrtp_msg::receiver& receiver, zrtp_session_t& session);
        };
    };
};

namespace uvg_rtp = uvgrtp;
