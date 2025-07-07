#pragma once

#include "uvgrtp/export.hh"


#include <stdint.h>



/* Currently this class uses millisecond precision, but it should be updated to use microseconds.
 * The RTP spec offers a microsecond precision in things like jiffies so we should offer it to users */

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

        UVGRTP_EXPORT uint64_t jiffies_to_ms(uint64_t jiffies);
    }
}

namespace uvg_rtp = uvgrtp;
