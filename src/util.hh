#pragma once

#include <cstdint>
#include <cstddef>

class RTPConnection;

const int MAX_PACKET      = 65536;
const int MAX_PAYLOAD     = 1000;

const size_t RTP_HEADER_SIZE      = 12;
const size_t HEVC_RTP_HEADER_SIZE =  2;
const size_t HEVC_FU_HEADER_SIZE  =  1;

enum RTP_ERROR {
    RTP_OK            =  0,
    RTP_GENERIC_ERROR = -1,
    RTP_SOCKET_ERROR  = -2,
    RTP_BIND_ERROR    = -3,
    RTP_INVALID_VALUE = -4,
    RTP_SEND_ERROR    = -5,
};

typedef enum RTP_FORMAT {
    RTP_FORMAT_GENERIC = 0,
    RTP_FORMAT_HEVC    = 96,
    RTP_FORMAT_OPUS    = 97,
} rtp_format_t;


static inline uint64_t rtpGetUniqueId()
{
    static uint64_t i = 1;
    return i++;
}
