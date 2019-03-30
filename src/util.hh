#pragma once

#include <cstdint>

class RTPConnection;

const int MAX_PACKET  = 65536;
const int MAX_PAYLOAD = 1000;

enum RTP_ERROR {
  RTP_SOCKET_ERROR = -1,
  RTP_BIND_ERROR   = -2,
};

typedef enum RTP_FORMAT {
    RTP_FORMAT_GENERIC = 0,
    RTP_FORMAT_HEVC    = 96,
    RTP_FORMAT_OPUS    = 97,
} rtp_format_t;


uint64_t rtpGetUniqueId();
int rtpRecvData(RTPConnection *conn);
