#pragma once

#include "uvgrtp/util.hh"
#include <string>

namespace uvgrtp {

    class socketfactory {

        public:
            socketfactory();
            ~socketfactory();

            void set_local_interface(std::string local_addr, uint16_t local_port);
        private:
            std::string local_address_;
            uint16_t local_port_;

    };
}
