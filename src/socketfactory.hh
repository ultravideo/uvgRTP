#pragma once

#include "uvgrtp/util.hh"
#include <string>
#include <memory>

namespace uvgrtp {

    class socket;
    class socketfactory {

        public:
            socketfactory();
            ~socketfactory();

            rtp_error_t set_local_interface(std::string local_addr, uint16_t local_port);
        private:
            std::string local_address_;
            uint16_t local_port_;
            bool ipv6_;
            std::shared_ptr<uvgrtp::socket> socket_;

    };
}
