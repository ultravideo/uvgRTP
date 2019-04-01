#pragma once

#include <thread>

#include "conn.hh"
#include "rtp_generic.hh"

class RTPReader : public RTPConnection {

public:
    RTPReader(std::string srcAddr, int srcPort);
    ~RTPReader();

    // TODO
    RTPGeneric::GenericFrame *pullFrame();

    // open socket and start runner_
    int start();

    bool active();

    uint8_t *getInPacketBuffer() const;
    uint32_t getInPacketBufferLength() const;

    void addOutgoingFrame(RTPGeneric::GenericFrame *frame);

private:
    // TODO implement ring buffer
    bool active_;

    // connection-related stuff
    std::string srcAddr_;
    int srcPort_;

    // receiver thread related stuff
    std::thread *runner_;
    uint8_t *inPacketBuffer_; /* Buffer for incoming packet (MAX_PACKET) */
    uint32_t inPacketBufferLen_;

    std::vector<RTPGeneric::GenericFrame *>  framesOut_;
    std::mutex framesMtx_;
};
