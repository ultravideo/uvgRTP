#pragma once

#include <cstdint>
#include <cstddef>
#include <cstdio>

const int MAX_PACKET      = 65536;
const int MAX_PAYLOAD     = 1000;

typedef enum RTP_ERROR {
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
