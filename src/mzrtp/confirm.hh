#pragma once

#include "frame.hh"
#include "util.hh"

#include "mzrtp/defines.hh"
#include "mzrtp/receiver.hh"

namespace kvz_rtp {

    namespace zrtp_msg {

        PACKED_STRUCT(zrtp_confirm) {
            zrtp_msg msg_start;

            uint8_t confirm_mac[8];
            uint8_t cfb_iv[16];

            /* encrypted portion starts */
            uint8_t hash[32];

            uint16_t unused:15;
            uint16_t sig_len:9; /* signature length*/
            uint8_t zeros:4;
            uint8_t e:1;        /*  */
            uint8_t v:1;        /*  */
            uint8_t d:1;        /*  */
            uint8_t a:1;        /*  */

            uint32_t cache_expr; /* cache expiration interval */
            /* encrypted portion ends */

            uint32_t crc;
        };

        class confirm {
            public:
                confirm(zrtp_session_t& session, int part);
                ~confirm();

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
