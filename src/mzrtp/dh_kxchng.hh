#pragma once

#include "frame.hh"
#include "util.hh"

#include "mzrtp/defines.hh"
#include "mzrtp/receiver.hh"

namespace kvz_rtp {

    namespace zrtp_msg {

        struct zrtp_dh {
            zrtp_msg msg_start;
            uint32_t hash[8];
            uint8_t rs1_id[8];
            uint8_t rs2_id[8];
            uint8_t aux_secret[8];
            uint8_t pbx_secret[8];
            uint8_t pk[384];
            uint8_t mac[8];
            uint32_t crc;
        };

        class dh_key_exchange {
            public:
                dh_key_exchange(zrtp_session_t& session);
                ~dh_key_exchange();

                /* TODO:  */
                rtp_error_t send_msg(socket_t& socket, sockaddr_in& addr);

                /* TODO:  */
                rtp_error_t parse_msg(kvz_rtp::zrtp_msg::receiver& receiver, zrtp_session_t& session);

                /* TODO:  */
                rtp_error_t set_role(zrtp_session_t& session, int part);

            private:
                kvz_rtp::frame::zrtp_frame *frame_;
                kvz_rtp::frame::zrtp_frame *rframe_;
                size_t len_, rlen_;

        };
    };
};
