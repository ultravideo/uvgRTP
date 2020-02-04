#pragma once

#include "util.hh"
#include "frame.hh"
#include "mzrtp/defines.hh"
#include "mzrtp/receiver.hh"

namespace kvz_rtp {

    namespace zrtp_msg {

        PACKED_STRUCT(zrtp_error) {
            zrtp_msg msg_start;
            uint32_t error;
            uint32_t crc;
        };

        class error {
            public:
                error(int error_code);
                ~error();

                rtp_error_t send_msg(socket_t& socket, sockaddr_in& addr);

                rtp_error_t parse_msg(kvz_rtp::zrtp_msg::receiver& receiver);

            private:
                kvz_rtp::frame::zrtp_frame *frame_;
                size_t len_;
        };

    };
};
