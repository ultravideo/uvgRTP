#pragma once

#include "clock_internal.hh"
#include "debug.hh"

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
int uvgrtp::clock::gettimeofday(struct timeval* tp, struct timezone* tzp)
{
    if (tzp != nullptr)
    {
        UVG_LOG_ERROR("Timezone not supported");
        return -1;
    }

    // https://stackoverflow.com/questions/10905892/equivalent-of-gettimeday-for-windows

    // Note: some broken versions only have 8 trailing zero's, the correct epoch has 9 trailing zero's
    // This magic number is the number of 100 nanosecond intervals since January 1, 1601 (UTC)
    // until 00:00:00 January 1, 1970
    static const uint64_t epoch = ((uint64_t)116444736000000000ULL);

    // TODO: Why do we have two epochs defined?

    SYSTEMTIME  system_time;
    FILETIME    file_time;
    uint64_t    time;

    GetSystemTime(&system_time);
    SystemTimeToFileTime(&system_time, &file_time);
    time = ((uint64_t)file_time.dwLowDateTime);
    time += ((uint64_t)file_time.dwHighDateTime) << 32;

    tp->tv_sec = (long)((time - epoch) / 10000000L);
    tp->tv_usec = (long)(system_time.wMilliseconds * 1000);
    return 0;
}
#endif
