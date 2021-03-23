#pragma once

#include "frame.hh"
#include "socket.hh"
#include "util.hh"

#include "zrtp/defines.hh"
#include "zrtp/zrtp_receiver.hh"

namespace uvgrtp {

    typedef struct capabilities zrtp_capab_t;
    typedef struct zrtp_session zrtp_session_t;

    namespace zrtp_msg {

        PACK(struct zrtp_hello {
            zrtp_msg msg_start;

            uint32_t version;
            uint32_t client[4];
            uint32_t hash[8];
            uint32_t zid[3];

            uint8_t zero:1;
            uint8_t s:1;
            uint8_t m:1;
            uint8_t p:1;
            uint8_t unused;
            uint8_t hc:4;
            uint8_t cc:4;
            uint8_t ac:4;
            uint8_t kc:4;
            uint8_t sc:4;

            uint64_t mac;
            uint32_t crc;
        });

        class hello {
            public:
                hello(zrtp_session_t& session);
                ~hello();

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
