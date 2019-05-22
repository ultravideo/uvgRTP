#ifdef _WIN32
// TODO
#else
#include <arpa/inet.h>
#endif

#include <stdint.h>
#include <cstring>
#include <iostream>

#include "debug.hh"
#include "conn.hh"
#include "rtp_generic.hh"
#include "send.hh"
#include "util.hh"
#include "writer.hh"

// TODO implement frame splitting if dataLen > MTU
// TODO write timestamp to RTP header
int RTPGeneric::pushGenericFrame(RTPConnection *conn, uint8_t *data, size_t dataLen, uint32_t timestamp)
{
    int ret;

    if ((ret = RTPSender::writeRTPHeader(conn)) != RTP_OK) {
        LOG_ERROR("Failed to write RTP Header for Opus frame!");
        return ret;
    }

    return RTPSender::writePayload(conn, data, dataLen);
}
