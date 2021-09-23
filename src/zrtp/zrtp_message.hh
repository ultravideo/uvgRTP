#pragma once

#include "defines.hh"
#include "util.hh"
#include "../zrtp.hh"
#include "frame.hh"


namespace uvgrtp {

    namespace zrtp_msg {

        class zrtp_message {
        public:
            zrtp_message();
            ~zrtp_message();

            rtp_error_t send_msg(uvgrtp::socket* socket, sockaddr_in& addr);

            /* TODO: description */
            virtual rtp_error_t parse_msg(uvgrtp::zrtp_msg::receiver& receiver,
                zrtp_session_t& session) = 0;

        protected:

            void allocate_frame(size_t frame_size);
            void allocate_rframe(size_t frame_size);

            void set_zrtp_start_base(uvgrtp::zrtp_msg::zrtp_msg& start, std::string msgblock);

            void set_zrtp_start(uvgrtp::zrtp_msg::zrtp_msg& start, zrtp_session_t& session,
                 std::string msgblock);

            uvgrtp::frame::zrtp_frame* frame_;
            uvgrtp::frame::zrtp_frame* rframe_;
            size_t len_;
            size_t rlen_;
        };
    };
};


namespace uvg_rtp = uvgrtp;