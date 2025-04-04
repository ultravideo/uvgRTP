#pragma once

#include <chrono>

#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#else
#include <sys/time.h>
#endif


namespace uvgrtp
{
    namespace clock {
        /* high-resolution clock */
        namespace hrc {
            typedef std::chrono::high_resolution_clock::time_point hrc_t;

            hrc_t now();

            /* the result is in milliseconds */
            uint64_t diff(hrc_t hrc1, hrc_t hrc2);

            /* the result is in milliseconds */
            uint64_t diff_now(hrc_t then);

            uint64_t diff_now_us(hrc_t& then);
        }

        uint64_t ms_to_jiffies(uint64_t ms);

#ifdef _WIN32
        int gettimeofday(struct timeval* tp, struct timezone* tzp);
#endif
    }
}
