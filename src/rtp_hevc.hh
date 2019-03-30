#pragma once

#include "rtp_generic.hh"

namespace RTPHevc {
    int pushHevcFrame(RTPConnection *conn, uint8_t *data, uint32_t dataLen, uint32_t timestamp);
};
