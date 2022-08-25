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

static inline void uvgrtp_debug(const char *level, const char *function, const char *s, ...)
{
    va_list args;
    va_start(args, s);
    char fmt[256] = {0};
    snprintf(fmt, sizeof(fmt), "[uvgRTP][%s][%s::%s] %s\n", level,
             "", function, s);
    vfprintf(stderr, fmt, args);
    va_end(args);
}

#ifndef NDEBUG
#define UVG_LOG_DEBUG(...) uvgrtp_debug(LOG_LEVEL_DEBUG, __func__, __VA_ARGS__)
#else
#define UVG_LOG_DEBUG(...) ;
#endif

#ifdef __RTP_SILENT__
#define UVG_LOG_ERROR(...) ;
#define UVG_LOG_WARN(...) ;
#define UVG_LOG_INFO(...) ;
#undef UVG_LOG_DEBUG
#define UVG_LOG_DEBUG(...) ;
#else
#define UVG_LOG_ERROR(...) uvgrtp_debug(LOG_LEVEL_ERROR, __func__, __VA_ARGS__)
#define UVG_LOG_WARN(...)  uvgrtp_debug(LOG_LEVEL_WARN,  __func__, __VA_ARGS__)
#define UVG_LOG_INFO(...)  uvgrtp_debug(LOG_LEVEL_INFO,  __func__, __VA_ARGS__)
#endif

static inline void log_platform_error(const char *aux)
{
#ifndef _WIN32
        if (aux) {
            UVG_LOG_ERROR("%s: %s %d\n", aux, strerror(errno), errno);
        } else {
            UVG_LOG_ERROR("%s %d\n", strerror(errno), errno);
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
        UVG_LOG_ERROR("%s: %ls %d\n", aux, s, WSAGetLastError());
    } else {
        UVG_LOG_ERROR("%ls %d\n", s, WSAGetLastError());
    }
    LocalFree(s);
#endif
}
