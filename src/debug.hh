#pragma once

#include <cstdio>
#include <cstdarg>
#include <string>

// TODO constexpr??
inline const char *className(const std::string& prettyFunction)
{
    size_t colons = prettyFunction.find("::");
    if (colons == std::string::npos)
        return "";

    size_t begin = prettyFunction.substr(0,colons).rfind(" ") + 1;
    size_t end = colons - begin;

    return prettyFunction.substr(begin,end).c_str();
}

#ifdef _WIN32

static inline void win_get_last_error(void)
{
#if 0
    wchar_t *s = NULL;
    FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL, WSAGetLastError(),
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPWSTR)&s, 0, NULL
    );
    fprintf(stderr, "%S %d\n", s, WSAGetLastError());
    LocalFree(s);
#endif
}

#define LOG_LEVEL_ERROR "ERROR"
#define LOG_LEVEL_WARN  "WARNING"
#define LOG_LEVEL_INFO  "INFO"
#else
#define LOG_LEVEL_ERROR "\x1b[31mERROR\x1b[0m"
#define LOG_LEVEL_WARN  "\x1b[33mWARNING\x1b[0m"
#define LOG_LEVEL_INFO  "\x1b[34mINFO\x1b[0m"
#endif
#define LOG_LEVEL_DEBUG "DEBUG"

#define debug(level, fmt, ...) \
	fprintf(stderr, "[RTPLIB][%s][%s::%s] " fmt "\n", level, \
            className(__PRETTY_FUNCTION__), __func__, ##__VA_ARGS__)

#ifndef NDEBUG
#define LOG_DEBUG(fmt,  ...) debug(LOG_LEVEL_DEBUG,  fmt, ##__VA_ARGS__)
#else
#define LOG_DEBUG(fmt,  ...) ;
#endif

#ifdef __RTP_SILENT__
#define LOG_ERROR(fmt,  ...) ;
#define LOG_WARN(fmt,   ...) ;
#define LOG_INFO(fmt,   ...) ;
#undef LOG_DEBUG
#define LOG_DEBUG(fmt,  ...) ;
#else
#define LOG_ERROR(fmt,  ...) debug(LOG_LEVEL_ERROR, fmt, ##__VA_ARGS__)
#define LOG_WARN(fmt,   ...) debug(LOG_LEVEL_WARN,  fmt, ##__VA_ARGS__)
#define LOG_INFO(fmt,   ...) debug(LOG_LEVEL_INFO,  fmt, ##__VA_ARGS__)
#endif
