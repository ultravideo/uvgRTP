#pragma once

namespace uvg_rtp {
    namespace mingw {
        int inet_pton(int af, const char *src, struct in_addr *dst);
    };
};
