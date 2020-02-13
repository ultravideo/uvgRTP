#ifdef __RTP_CRYPTO__
#pragma once

#include "util.hh"
#include "frame.hh"
#include "mzrtp/defines.hh"
#include "mzrtp/receiver.hh"

namespace kvz_rtp {

    namespace zrtp_msg {

        PACKED_STRUCT(zrtp_hello_ack) {
            zrtp_msg msg_start;
            uint32_t crc;
        };

        class hello_ack {
            public:
                hello_ack();
                ~hello_ack();

                rtp_error_t send_msg(socket_t& socket, sockaddr_in& addr);

                rtp_error_t parse_msg(kvz_rtp::zrtp_msg::receiver& receiver);

            private:
                kvz_rtp::frame::zrtp_frame *frame_;
                size_t len_;
        };

    };
};
#endif
