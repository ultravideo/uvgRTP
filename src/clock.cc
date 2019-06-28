#ifdef _WIN32
/* TODO:  */
#else
#include <sys/time.h>
#endif

#include "clock.hh"

static inline uint32_t ntp_diff_ms(uint64_t t1, uint64_t t2)
{
    uint32_t s_diff  = (t1 >> 32) - (t2 >> 32);
    uint32_t us_diff = (t1 & 0xffffffff) - (t2 & 0xffffffff);

    return s_diff * 1000 + (us_diff / 1000000UL);
}

uint64_t kvz_rtp::clock::now_ntp()
{
    static const uint64_t EPOCH = 2208988800ULL;
    static const uint64_t NTP_SCALE_FRAC = 4294967296ULL;

    struct timeval tv;
    gettimeofday(&tv, NULL);

    uint64_t tv_ntp, tv_usecs;

    tv_ntp = tv.tv_sec + EPOCH;
    tv_usecs = (NTP_SCALE_FRAC * tv.tv_usec) / 1000000UL;

    return (tv_ntp << 32) | tv_usecs;
}

kvz_rtp::clock::tp_t kvz_rtp::clock::now_high_res()
{
    return std::chrono::high_resolution_clock::now();
}

uint64_t kvz_rtp::clock::diff_tp_ms(kvz_rtp::clock::tp_t t1, kvz_rtp::clock::tp_t t2)
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t2).count();
}

uint64_t kvz_rtp::clock::diff_tp_s(kvz_rtp::clock::tp_t t1, kvz_rtp::clock::tp_t t2)
{
    return std::chrono::duration_cast<std::chrono::seconds>(t1 - t2).count();
}

uint64_t kvz_rtp::clock::diff_ntp_now_ms(uint64_t then)
{
    return diff_ntp_ms(kvz_rtp::clock::now_ntp(), then);
}

uint64_t kvz_rtp::clock::diff_tp_now_ms(tp_t then)
{
    uint64_t diff = (uint64_t)std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::high_resolution_clock::now() - then
    ).count();

    return (uint64_t)diff;
}

uint64_t kvz_rtp::clock::diff_tp_now_s(tp_t then)
{
    uint64_t diff = (uint64_t)std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::high_resolution_clock::now() -  then
    ).count();

    return (uint64_t)diff;
}

uint32_t kvz_rtp::clock::now_epoch()
{
    return 0;
}

uint32_t kvz_rtp::clock::now_epoch_ms()
{
    return 0;
}

uint64_t kvz_rtp::clock::diff_ntp_ms(uint64_t ntp1, uint64_t ntp2)
{
    uint32_t s_diff  = (ntp1 >> 32) - (ntp2 >> 32);
    uint32_t us_diff = (ntp1 & 0xffffffff) - (ntp2 & 0xffffffff);

    return s_diff * 1000 + (us_diff / 1000000UL);
}

uint64_t kvz_rtp::clock::ms_to_jiffies(uint64_t ms)
{
    return ((double)ms / 1000) * 65536;
}

uint64_t kvz_rtp::clock::jiffies_to_ms(uint64_t jiffies)
{
    return ((double)jiffies / 65536) * 1000;
}
