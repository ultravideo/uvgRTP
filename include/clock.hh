#pragma once

#include <chrono>

namespace uvg_rtp {
    namespace clock {

        /* network time protocol */
        namespace ntp {
            uint64_t now();

            /* return the difference of ntp timestamps in milliseconds */
            uint64_t diff(uint64_t ntp1, uint64_t ntp2);

            /* Calculate the difference between now
             * (a wall clock reading when the function is called) and "then"
             *
             * The result is in milliseconds */
            uint64_t diff_now(uint64_t then);
        };

        /* high-resolution clock */
        namespace hrc {
            typedef std::chrono::high_resolution_clock::time_point hrc_t;

            hrc_t now();

            /* the result is in milliseconds */
            uint64_t diff(hrc_t hrc1, hrc_t hrc2);

            /* the result is in milliseconds */
            uint64_t diff_now(hrc_t then);

            uint64_t diff_now_us(hrc_t& then);
        };

        uint64_t ms_to_jiffies(uint64_t ms);
        uint64_t jiffies_to_ms(uint64_t jiffies);

#ifdef _WIN32
        int gettimeofday(struct timeval *tp, struct timezone *tzp);
#endif
    };
};
