#pragma once

#include "uvgrtp/util.hh"
#include <string>

namespace uvgrtp {

    class socketfactory {

        public:
            socketfactory();

        private:
            std::string local_address_;
            uint16_t local_port_;

    };
}
