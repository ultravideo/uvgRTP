#pragma once

#include "uvgrtp_export.hh"

#include <stdint.h>

namespace uvgrtp {
    namespace clock {

        /* network time protocol */
        namespace ntp {
            /**
             * \ingroup CORE_API
             * \brief Get current time in NTP units
             *
             * \return NTP timestamp
             */
            UVGRTP_EXPORT uint64_t now();

            /**
             * \ingroup CORE_API
             * \brief Calculate the time difference of two NTP times
             *
             * The second timestamp is subtracted from the first one
             *
             * \param ntp1 First NTP timestamp
             * \param ntp2 Second NTP timestamp
             *
             * \return Difference of the timestamps in milliseconds
             */
            UVGRTP_EXPORT uint64_t diff(uint64_t ntp1, uint64_t ntp2);

            /**
             * \ingroup CORE_API
             * \brief Calculate the time difference of two NTP times
             *
             * \details This function calls uvgrtp::clock::ntp::now()
             * and then subtracts the input parameter from that timestamp value.
             *
             * \param then NTP timestamp
             *
             * \return Difference of the timestamps in milliseconds
             */
            UVGRTP_EXPORT uint64_t diff_now(uint64_t then);
        }
    }
}

namespace uvg_rtp = uvgrtp;
