#pragma once

#include "uvgrtp/util.hh"
#include <string>
#include <memory>

namespace uvgrtp {

    class socket;
    class socketfactory {

        public:
            socketfactory(int rce_flags);
            ~socketfactory();

            rtp_error_t set_local_interface(std::string local_addr);
            rtp_error_t bind_local_socket(uint16_t local_port);

            bool get_local_bound() const;
            std::shared_ptr<uvgrtp::socket> get_socket_ptr() const;

        private:
            int rce_flags_;
            std::string local_address_;
            uint16_t local_port_;
            bool ipv6_;
            std::shared_ptr<uvgrtp::socket> socket_;
            bool local_bound_;

    };
}
