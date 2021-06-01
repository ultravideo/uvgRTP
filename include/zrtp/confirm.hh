#pragma once


#include "zrtp/zrtp_receiver.hh"
#include "zrtp.hh"
#include "zrtp/defines.hh"

#include "frame.hh"
#include "socket.hh"
#include "util.hh"


namespace uvgrtp {

    namespace zrtp_msg {

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

            uint32_t cache_expr; /* cache expiration interval */
            /* encrypted portion ends */

            uint32_t crc;
        });

        class confirm {
            public:
                confirm(zrtp_session_t& session, int part);
                ~confirm();

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
