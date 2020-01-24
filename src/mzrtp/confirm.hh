#pragma once

#include "frame.hh"
#include "util.hh"

#include "mzrtp/defines.hh"
#include "mzrtp/receiver.hh"

namespace kvz_rtp {

    namespace zrtp_msg {

        struct zrtp_confirm {
            zrtp_msg msg_start;
        };

        class confirm {
            public:
                confirm(int part);
                ~confirm();

                /* TODO:  */
                rtp_error_t send_msg(socket_t& socket, sockaddr_in& addr);

                /* TODO:  */
                rtp_error_t parse_msg(kvz_rtp::zrtp_msg::receiver& receiver);

            private:
                kvz_rtp::frame::zrtp_frame *frame_;
                kvz_rtp::frame::zrtp_frame *rframe_;
                size_t len_, rlen_;
        };
    };
};
