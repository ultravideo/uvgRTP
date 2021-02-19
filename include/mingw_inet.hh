#pragma once

namespace uvgrtp {
    namespace mingw {
        int inet_pton(int af, const char *src, struct in_addr *dst);
    };
};

namespace uvg_rtp = uvgrtp;
