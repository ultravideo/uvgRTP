#pragma once

#include <string>

namespace uvgrtp {
    namespace hostname {
        std::string get_hostname();
        std::string get_username();
    }
}

namespace uvg_rtp = uvgrtp;
