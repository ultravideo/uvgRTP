#pragma once

#include "zrtp/defines.hh"
#include "zrtp/zrtp_receiver.hh"

#include "frame.hh"
#include "socket.hh"
#include "util.hh"



namespace uvgrtp {

    typedef struct zrtp_session zrtp_session_t;

    namespace zrtp_msg {

        PACK(struct zrtp_dh {
            zrtp_msg msg_start;
            uint32_t hash[8];
            uint8_t rs1_id[8];
            uint8_t rs2_id[8];
            uint8_t aux_secret[8];
            uint8_t pbx_secret[8];
            uint8_t pk[384];
            uint8_t mac[8];
            uint32_t crc;
        });

        class dh_key_exchange {
            public:
                dh_key_exchange(zrtp_session_t& session, int part);
                dh_key_exchange(struct zrtp_dh *dh);
                ~dh_key_exchange();

                /* TODO:  */
                rtp_error_t send_msg(uvgrtp::socket *socket, sockaddr_in& addr);

                /* TODO:  */
                rtp_error_t parse_msg(uvgrtp::zrtp_msg::receiver& receiver, zrtp_session_t& session);

            private:
                uvgrtp::frame::zrtp_frame *frame_;
                uvgrtp::frame::zrtp_frame *rframe_;
                size_t len_, rlen_;

        };
    };
};

namespace uvg_rtp = uvgrtp;
