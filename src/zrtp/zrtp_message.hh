#pragma once

#include "zrtp_receiver.hh"
#include "defines.hh"

#include "uvgrtp/frame.hh"
#include "uvgrtp/util.hh"

#include <memory>
#ifdef _WIN32
#include <ws2ipdef.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#endif

namespace uvgrtp {

    class socket;

    namespace zrtp_msg {

        class zrtp_message {
        public:
            zrtp_message();
            ~zrtp_message();

            rtp_error_t send_msg(std::shared_ptr<uvgrtp::socket> socket, sockaddr_in& addr, sockaddr_in6& addr6);

            static ssize_t header_length_to_packet(uint16_t header_len);
            static uint16_t packet_to_header_len(ssize_t packet);

        protected:

            void allocate_frame(size_t frame_size);
            void allocate_rframe(size_t frame_size);

            void set_zrtp_start_base(uvgrtp::zrtp_msg::zrtp_msg& start, std::string msgblock);

            void set_zrtp_start(uvgrtp::zrtp_msg::zrtp_msg& start, zrtp_session_t& session,
                 std::string msgblock);

            void* frame_;
            void* rframe_;
            size_t len_;
            size_t rlen_;
        };
    }
}


namespace uvg_rtp = uvgrtp;
