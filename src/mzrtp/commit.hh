#pragma once

#include "frame.hh"
#include "util.hh"

#include "mzrtp/defines.hh"
#include "mzrtp/receiver.hh"

namespace kvz_rtp {

    typedef struct zrtp_session zrtp_session_t;

    namespace zrtp_msg {

        /* DH Commit Message */
        PACKED_STRUCT(zrtp_commit) {
            zrtp_msg msg_start;

            uint32_t hash[8];
            uint32_t zid[3];
            uint32_t hash_algo;
            uint32_t cipher_algo;
            uint32_t auth_tag_type;
            uint32_t key_agreement_type;
            uint32_t sas_type;

            uint32_t hvi[8];
            uint32_t mac[2];
        };

        class commit {
            public:
                commit(zrtp_session_t& session);
                ~commit();

                /* TODO:  */
                rtp_error_t send_msg(socket_t& socket, sockaddr_in& addr);

                /* TODO:  */
                rtp_error_t parse_msg(kvz_rtp::zrtp_msg::receiver& receiver, zrtp_session_t& session);

            private:
                kvz_rtp::frame::zrtp_frame *frame_;
                kvz_rtp::frame::zrtp_frame *rframe_;
                size_t len_, rlen_;

        };
    };
};
