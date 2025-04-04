#include "uvgrtp/clock.hh"

#include "clock_internal.hh"
#include "debug.hh"

#include <stdio.h>

static const uint64_t EPOCH = 2208988800ULL;
static const uint64_t NTP_SCALE_FRAC = 4294967296ULL;

static inline uint32_t ntp_diff_ms(uint64_t older, uint64_t newer)
{
    if (older > newer)
    {
        UVG_LOG_ERROR("Older timestamp is actually newer");
    }

    uint32_t s1  = (older >> 32) & 0xffffffff;
    uint32_t s2  = (newer >> 32) & 0xffffffff;
    uint64_t us1 = ((older & 0xffffffff) * 1000000UL) / NTP_SCALE_FRAC;
    uint64_t us2 = ((newer & 0xffffffff) * 1000000UL) / NTP_SCALE_FRAC;

    uint64_t r = (((uint64_t)(s2 - s1) * 1000000) + ((us2 - us1))) / 1000;

    if (r > UINT32_MAX)
    {
        UVG_LOG_ERROR("NTP difference is too large: %llu. Limiting value", r);
        r = UINT32_MAX;
    }

    return (uint32_t)r;
}

uint64_t uvgrtp::clock::ntp::now()
{
    struct timeval tv;
#ifdef _WIN32
    uvgrtp::clock::gettimeofday(&tv, NULL);
#else
    gettimeofday(&tv, NULL);
#endif

    uint64_t tv_ntp = tv.tv_sec + EPOCH;
    uint64_t tv_usecs = (uint64_t)((float)(NTP_SCALE_FRAC * tv.tv_usec) / 1000000.f);

    return (tv_ntp << 32) | tv_usecs;
}

uint64_t uvgrtp::clock::ntp::diff(uint64_t ntp1, uint64_t ntp2)
{
    return ntp_diff_ms(ntp1, ntp2);
}

uint64_t uvgrtp::clock::ntp::diff_now(uint64_t then)
{
    uint64_t now = uvgrtp::clock::ntp::now();

    return ntp_diff_ms(then, now);
}

uint64_t uvgrtp::clock::jiffies_to_ms(uint64_t jiffies)
{
    return (uint64_t)(((double)jiffies / 65536) * 1000);
}

