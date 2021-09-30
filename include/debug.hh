#pragma once

#ifdef _WIN32
#include <winsock2.h>
#include <winsock.h>
#include <winbase.h>
#endif

#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <string>

// TODO constexpr??
inline const std::string className(const std::string& prettyFunction)
{
    size_t colons = prettyFunction.find("::");
    if (colons == std::string::npos)
        return "";

    size_t begin = prettyFunction.substr(0,colons).rfind(" ") + 1;
    size_t end = colons - begin;

    std::string partialString = prettyFunction.substr(begin, end);

    return partialString;
}

#ifdef _WIN32

static inline void win_get_last_error(void)
{
    wchar_t *s = NULL;
    FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL, WSAGetLastError(),
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPWSTR)&s, 0, NULL
    );
    fprintf(stderr, "%S %d\n", s, WSAGetLastError());
    LocalFree(s);
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

#define uvgrtp_debug(level, fmt, ...) \
    fprintf(stderr, "[uvgRTP][%s][%s::%s] " fmt "\n", level, \
            "", __func__, ##__VA_ARGS__)

#ifndef NDEBUG
#define LOG_DEBUG(fmt,  ...) uvgrtp_debug(LOG_LEVEL_DEBUG,  fmt, ##__VA_ARGS__)
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
#define LOG_ERROR(fmt,  ...) uvgrtp_debug(LOG_LEVEL_ERROR, fmt, ##__VA_ARGS__)
#define LOG_WARN(fmt,   ...) uvgrtp_debug(LOG_LEVEL_WARN,  fmt, ##__VA_ARGS__)
#define LOG_INFO(fmt,   ...) uvgrtp_debug(LOG_LEVEL_INFO,  fmt, ##__VA_ARGS__)
#endif

static inline void log_platform_error(const char *aux)
{
#ifndef _WIN32
        if (aux) {
            LOG_ERROR("%s: %s %d\n", aux, strerror(errno), errno);
        } else {
            LOG_ERROR("%s %d\n", strerror(errno), errno);
        }
#else
    wchar_t *s = NULL;
    FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL, WSAGetLastError(),
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPWSTR)&s, 0, NULL
    );

    if (aux) {
        LOG_ERROR("%s: %ls %d\n", aux, s, WSAGetLastError());
    } else {
        LOG_ERROR("%ls %d\n", s, WSAGetLastError());
    }
    LocalFree(s);
#endif
}
