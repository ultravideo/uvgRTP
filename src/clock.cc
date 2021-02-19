#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#else
#include <sys/time.h>
#endif

#include "clock.hh"
#include <stdio.h>

static const uint64_t EPOCH = 2208988800ULL;
static const uint64_t NTP_SCALE_FRAC = 4294967296ULL;

static inline uint32_t ntp_diff_ms(uint64_t t1, uint64_t t2)
{
    uint32_t s1  = (t1 >> 32) & 0xffffffff;
    uint32_t s2  = (t2 >> 32) & 0xffffffff;
    uint64_t us1 = ((t1 & 0xffffffff) * 1000000UL) / NTP_SCALE_FRAC;
    uint64_t us2 = ((t2 & 0xffffffff) * 1000000UL) / NTP_SCALE_FRAC;

    return (((s1 - s2) * 1000000) + ((us1 - us2))) / 1000;
}

uint64_t uvgrtp::clock::ntp::now()
{
    struct timeval tv;
#ifdef _WIN32
    uvgrtp::clock::gettimeofday(&tv, NULL);
#else
    gettimeofday(&tv, NULL);
#endif

    uint64_t tv_ntp, tv_usecs;

    tv_ntp = tv.tv_sec + EPOCH;
    tv_usecs = (float)(NTP_SCALE_FRAC * tv.tv_usec) / (float)1000000UL;

    return (tv_ntp << 32) | tv_usecs;
}

uint64_t uvgrtp::clock::ntp::diff(uint64_t ntp1, uint64_t ntp2)
{
    return ntp_diff_ms(ntp1, ntp2);
}

uint64_t uvgrtp::clock::ntp::diff_now(uint64_t then)
{
    uint64_t now = uvgrtp::clock::ntp::now();

    return ntp_diff_ms(now, then);
}

uvgrtp::clock::hrc::hrc_t uvgrtp::clock::hrc::now()
{
    return std::chrono::high_resolution_clock::now();
}

uint64_t uvgrtp::clock::hrc::diff(hrc_t hrc1, hrc_t hrc2)
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(hrc1 - hrc2).count();
}

uint64_t uvgrtp::clock::hrc::diff_now(hrc_t then)
{
    uint64_t diff = (uint64_t)std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::high_resolution_clock::now() - then
    ).count();

    return diff;
}

uint64_t uvgrtp::clock::hrc::diff_now_us(hrc_t& then)
{
    uint64_t diff = (uint64_t)std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::high_resolution_clock::now() - then
    ).count();

    return diff;
}

uint64_t uvgrtp::clock::ms_to_jiffies(uint64_t ms)
{
    return (uint64_t)(((double)ms / 1000) * 65536);
}

uint64_t uvgrtp::clock::jiffies_to_ms(uint64_t jiffies)
{
    return (uint64_t)(((double)jiffies / 65536) * 1000);
}

#ifdef _WIN32
int uvgrtp::clock::gettimeofday(struct timeval * tp, struct timezone * tzp)
{
    // Note: some broken versions only have 8 trailing zero's, the correct epoch has 9 trailing zero's
    // This magic number is the number of 100 nanosecond intervals since January 1, 1601 (UTC)
    // until 00:00:00 January 1, 1970
    static const uint64_t EPOCH = ((uint64_t) 116444736000000000ULL);

    SYSTEMTIME  system_time;
    FILETIME    file_time;
    uint64_t    time;

    GetSystemTime( &system_time );
    SystemTimeToFileTime( &system_time, &file_time );
    time =  ((uint64_t)file_time.dwLowDateTime )      ;
    time += ((uint64_t)file_time.dwHighDateTime) << 32;

    tp->tv_sec  = (long) ((time - EPOCH) / 10000000L);
    tp->tv_usec = (long) (system_time.wMilliseconds * 1000);
    return 0;
}
#endif
