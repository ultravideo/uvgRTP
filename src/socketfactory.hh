#pragma once

#include "uvgrtp/util.hh"
#include <string>
#include <memory>
#include <vector>

namespace uvgrtp {

    class socket;
    class socketfactory {

        public:
            socketfactory(int rce_flags);
            ~socketfactory();

            rtp_error_t set_local_interface(std::string local_addr);
            rtp_error_t create_new_socket(uint16_t local_port);

            bool get_local_bound() const;
            std::shared_ptr<uvgrtp::socket> get_socket_ptr() const;

        private:

            int rce_flags_;
            std::string local_address_;
            std::vector<uint16_t> used_ports_;
            bool ipv6_;
            std::vector<std::shared_ptr<uvgrtp::socket>> used_sockets_;
            bool local_bound_;

    };
}
