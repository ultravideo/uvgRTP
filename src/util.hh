#pragma once

#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#else
#include <sys/time.h>
#endif

#include <algorithm>
#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <string>


#if defined(__MINGW32__) || defined(__MINGW64__) || defined(__linux__)
#define PACKED_STRUCT(name) \
    struct __attribute__((packed)) name
#else
#warning "structures are not packed!"
#define PACKED_STRUCT(name) struct name
#endif

#ifdef _WIN32
    typedef SOCKET socket_t;
#else
    typedef int socket_t;
#endif

const int MAX_PACKET      = 65536;
const int MAX_PAYLOAD     = 1441;

typedef enum RTP_ERROR {
    RTP_INTERRUPTED    =  2,
    RTP_NOT_READY      =  1,
    RTP_OK             =  0,
    RTP_GENERIC_ERROR  = -1,
    RTP_SOCKET_ERROR   = -2,
    RTP_BIND_ERROR     = -3,
    RTP_INVALID_VALUE  = -4,
    RTP_SEND_ERROR     = -5,
    RTP_MEMORY_ERROR   = -6,
    RTP_SSRC_COLLISION = -7,
} rtp_error_t;

typedef enum RTP_FORMAT {
    RTP_FORMAT_GENERIC = 0,
    RTP_FORMAT_HEVC    = 96,
    RTP_FORMAT_OPUS    = 97,
} rtp_format_t;

typedef enum RTP_FLAGS {
    RTP_NO_FLAGS = 0 << 0,

    /* TODO  */
    RTP_SLICE    = 1 << 0,

    /* TODO */
    RTP_MORE     = 1 << 1,

    /* Make a copy of "data" given to push_frame()
     *
     * This is an easy way out of the memory ownership/deallocation problem
     * for the application but can significantly damage the performance
     *
     * NOTE: Copying is necessary only when the following conditions are met:
     *   - SCD is enabled
     *   - Media format is such that it uses SCD (HEVC is the only for now)
     *   - Application hasn't provided a deallocation callback
     *   - Application doens't use unique_ptrs
     *
     * If all of those conditions are met, then it's advised to pass RTP_COPY.
     * Otherwise there might be a lot of leaked memory */
    RTP_COPY = 1 << 2,
} rtp_flags_t;

/* These flags are given when kvzRTP context is created */
enum RTP_CTX_ENABLE_FLAGS {
    RTP_CTX_NO_FLAGS           = 0 << 0,

    /* Use optimistic receiver (HEVC only) */
    RCE_OPTIMISTIC_RECEIVER    = 1 << 0,

    /* Enable system call dispatcher (HEVC only) */
    RCE_SYSTEM_CALL_DISPATCHER = 1 << 2,

    RCE_LAST
};

/* These options are given to configuration() */
enum RTP_CTX_CONFIGURATION_FLAGS {
    /* No configuration flags */
    RCC_NO_FLAGS                  = 0,

    /* How many packets can be fit into
     * probation zone until they overflow to separate frames.
     *
     * By default, probation zone is disabled
     *
     * NOTE: how many **packets**, not bytes */
    RCC_PROBATION_ZONE_SIZE       = 1,

    /* How many transactions can be cached for later use 
     * Caching transactions improves performance by reducing
     * the number of (de)allocations but increases the memory
     * footprint of the program
     *
     * By default, 10 transactions are cached */
    RCC_MAX_TRANSACTIONS          = 2,

    /* How many UDP packets does one transaction object hold.
     *
     * kvzRTP splits one input frame [argument of push_frame()] into
     * multiple UDP packets, each of size 1500 bytes. This UDP packets
     * are stored into a transaction object.
     *
     * Video with high bitrate may require large value for "RCC_MAX_MESSAGES" 
     *
     * By default, it is set to 500 (ie. one frame can take up to 500 * 1500 bytes) */
    RCC_MAX_MESSAGES              = 3,

    /* How many chunks each UDP packet can at most contain
     *
     * By default, this is set to 4 */
    RCC_MAX_CHUNKS_PER_MSG        = 4,

    /* How large is the receiver/sender UDP buffer size
     *
     * Default value is 4 MB
     *
     * For video with high bitrate, it is advisable to set this
     * to a high number to prevent OS from dropping packets */
    RCC_UDP_BUF_SIZE              = 5,

    RCC_LAST
};

/* see src/util.hh for more information */
typedef struct rtp_ctx_conf {
    int flags;
    ssize_t ctx_values[RCC_LAST];
} rtp_ctx_conf_t;

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
