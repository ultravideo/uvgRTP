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
            std::shared_ptr<uvgrtp::socket> create_new_socket();
            rtp_error_t bind_socket(std::shared_ptr<uvgrtp::socket> soc, uint16_t port);
            rtp_error_t bind_socket_anyip(std::shared_ptr<uvgrtp::socket> soc, uint16_t port);


            std::shared_ptr<uvgrtp::socket> get_socket_ptr() const;
            bool get_ipv6() const;

        private:

            int rce_flags_;
            std::string local_address_;
            std::vector<uint16_t> used_ports_;
            bool ipv6_;
            std::vector<std::shared_ptr<uvgrtp::socket>> used_sockets_;

    };
}
