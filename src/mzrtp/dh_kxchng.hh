#pragma once

#include "frame.hh"
#include "util.hh"

#include "mzrtp/defines.hh"
#include "mzrtp/receiver.hh"

namespace kvz_rtp {

    namespace zrtp_msg {

        struct zrtp_dh {
            zrtp_msg msg_start;
            uint32_t hash_image[8];
            uint32_t rs1_id[2];
            uint32_t rs2_id[2];
            uint32_t aux_secret[2];
            uint32_t pbx_secret[2];
            uint32_t pvr[8];
            uint32_t mac[2];
        };

        class dh_key_exchange {
            public:
                dh_key_exchange(int part);
                ~dh_key_exchange();

                /* TODO:  */
                rtp_error_t send_msg(socket_t& socket, sockaddr_in& addr);

                /* TODO:  */
                rtp_error_t parse_msg(kvz_rtp::zrtp_msg::receiver& receiver, kvz_rtp::zrtp_dh_t& dh);

            private:
                kvz_rtp::frame::zrtp_frame *frame_;
                kvz_rtp::frame::zrtp_frame *rframe_;
                size_t len_, rlen_;

        };
    };
};
