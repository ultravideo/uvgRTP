#ifdef _WIN32
#else
#include <arpa/inet.h>
#endif
#include <iostream>
#include <cstring>

#include "util.hh"
#include "conn.hh"
#include "rtp_generic.hh"

uint64_t rtpGetUniqueId()
{
    static uint64_t i = 1;
    return i++;
}
