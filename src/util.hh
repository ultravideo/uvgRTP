#pragma once

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/time.h>
#endif

#include <algorithm>
#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <string>

#ifdef _WIN32

/* TODO: make sure this works on windows too! */

#define PACKED_STRUCT_WIN(name) \
    __pragma(pack(push, 1)) \
    struct name \
    __pragma(pack(pop))
#else
#define PACKED_STRUCT(name) \
    struct __attribute__((packed)) name
#endif

const int MAX_PACKET      = 65536;
const int MAX_PAYLOAD     = 1000;

typedef enum RTP_ERROR {
    RTP_INTERRUPTED   =  2,
    RTP_NOT_READY     =  1,
    RTP_OK            =  0,
    RTP_GENERIC_ERROR = -1,
    RTP_SOCKET_ERROR  = -2,
    RTP_BIND_ERROR    = -3,
    RTP_INVALID_VALUE = -4,
    RTP_SEND_ERROR    = -5,
    RTP_MEMORY_ERROR  = -6,
} rtp_error_t;

typedef enum RTP_FORMAT {
    RTP_FORMAT_GENERIC = 0,
    RTP_FORMAT_HEVC    = 96,
    RTP_FORMAT_OPUS    = 97,
} rtp_format_t;

extern thread_local rtp_error_t rtp_errno;

static inline void hex_dump(uint8_t *buf, size_t len)
{
    if (!buf)
        return;

    for (size_t i = 0; i < len; i += 10) {
        fprintf(stderr, "\t");
        for (size_t k = i; k < i + 10; ++k) {
            fprintf(stderr, "0x%02x ", buf[k]);
        }
        fprintf(stderr, "\n");
    }
}

static inline void set_bytes(int *ptr, int nbytes)
{
    if (ptr)
        *ptr = nbytes;
}

static inline std::string generate_string(size_t length)
{
    auto randchar = []() -> char
    {
        const char charset[] =
        "0123456789"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz";
        const size_t max_index = (sizeof(charset) - 1);
        return charset[ rand() % max_index ];
    };

    std::string str(length, 0);
    std::generate_n(str.begin(), length, randchar);
    return str;
}

static inline uint32_t generate_rand_32()
{
    static bool init = false;

    if (!init) {
        srand(time(NULL));
        init = true;
    }

    return rand();
}
