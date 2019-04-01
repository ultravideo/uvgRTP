#pragma once

#include <stdint.h>

#include "conn.hh"

class RTPWriter : public RTPConnection {

public:
    RTPWriter(std::string dstAddr, int dstPort);
    ~RTPWriter();

    // open socket for sending frames
    int start();

    // TODO
    int pushFrame(uint8_t *data, uint32_t datalen, rtp_format_t fmt, uint32_t timestamp);

    // TODO
    int pushGenericFrameFrame(RTPGeneric::GenericFrame *frame);

    sockaddr_in getOutAddress();

private:
    std::string dstAddr_;
    int dstPort_;
    sockaddr_in addrOut_;
};
