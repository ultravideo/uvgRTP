#pragma once

#ifdef _WIN32
#include <winsock2.h>
#endif

namespace uvgrtp {
    namespace mingw {
        int inet_pton(int af, const char *src, struct in_addr *dst);
    }
}

namespace uvg_rtp = uvgrtp;
