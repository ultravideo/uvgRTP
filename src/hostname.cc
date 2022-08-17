#include "hostname.hh"

#include "debug.hh"

#ifdef _WIN32
//#include <windows.h>
//#include <winbase.h>
#else
#include <unistd.h>
#include <cstring>
#include <limits.h>
#include <errno.h>
#endif

#define NAME_MAXLEN 512

std::string uvgrtp::hostname::get_hostname()
{
#ifdef _WIN32
    char buffer[NAME_MAXLEN];
    DWORD bufCharCount = NAME_MAXLEN;

    if (!GetComputerName((TCHAR *)buffer, &bufCharCount))
        log_platform_error("GetComputerName() failed");

    return std::string(buffer);
#else
    char hostname[NAME_MAXLEN];

    if (gethostname(hostname, NAME_MAXLEN) != 0) {
        UVG_LOG_ERROR("%s", strerror(errno));
        return "";
    }

    return std::string(hostname);
#endif
}

std::string uvgrtp::hostname::get_username()
{
#ifdef _WIN32
    char buffer[NAME_MAXLEN];
    DWORD bufCharCount = NAME_MAXLEN;

    if (!GetUserName((TCHAR *)buffer, &bufCharCount)) {
        log_platform_error("GetUserName() failed");
        return "";
    }

    return std::string(buffer);
#else
    char username[NAME_MAXLEN];

    if (getlogin_r(username, NAME_MAXLEN) != 0) {
        UVG_LOG_ERROR("%s", strerror(errno));
        return "";
    }

    return std::string(username);
#endif
}
