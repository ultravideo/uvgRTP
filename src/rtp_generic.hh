#pragma once

#include "util.hh"

class RTPConnection;

namespace RTPGeneric {
    int pushGenericFrame(RTPConnection *conn, uint8_t *data, size_t dataLen, uint32_t timestamp);
};
