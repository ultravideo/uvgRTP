#pragma once

#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#else
#include <sys/time.h>
#endif


#include <chrono>

namespace uvgrtp {
    namespace clock {

        /* network time protocol */
        namespace ntp {
            /**
             * \brief Get current time in NTP units
             *
             * \return NTP timestamp
             */
            uint64_t now();

            /**
             * \brief Calculate the time difference of two NTP times
             *
             * The second timestamp is subtracted from the first one
             *
             * \param ntp1 First NTP timestamp
             * \param ntp2 Second NTP timestamp
             *
             * \return Difference of the timestamps in milliseconds
             */
            uint64_t diff(uint64_t ntp1, uint64_t ntp2);

            /**
             * \brief Calculate the time difference of two NTP times
             *
             * \details This function calls uvgrtp::clock::ntp::now()
             * and then subtracts the input parameter from that timestamp value.
             *
             * \param then NTP timestamp
             *
             * \return Difference of the timestamps in milliseconds
             */
            uint64_t diff_now(uint64_t then);
        }

        /// \cond DO_NOT_DOCUMENT
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
        uint64_t jiffies_to_ms(uint64_t jiffies);

#ifdef _WIN32
        int gettimeofday(struct timeval *tp, struct timezone *tzp);
#endif
        /// \endcond
    }
}

namespace uvg_rtp = uvgrtp;
