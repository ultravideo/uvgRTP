#pragma once

#include "frame.hh"
#include "util.hh"

#include "mzrtp/defines.hh"
#include "mzrtp/receiver.hh"

namespace kvz_rtp {

    typedef struct capabilities zrtp_capab_t;
    typedef struct zrtp_session zrtp_session_t;

    namespace zrtp_msg {

        PACKED_STRUCT(zrtp_hello) {
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
        };

        class hello {
            public:
                hello(zrtp_session_t& session);
                ~hello();

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
