#pragma once

#include <thread>

#include "conn.hh"
#include "frame.hh"

class RTPReader : public RTPConnection {

public:
    RTPReader(std::string srcAddr, int srcPort);
    ~RTPReader();

    /* 
     *
     * NOTE: this operation is blocking */
    RTPFrame::Frame *pullFrame();

    // open socket and start runner_
    int start();

    bool active();

    bool receiveHookInstalled();
    void receiveHook(RTPFrame::Frame *frame);
    void installReceiveHook(void *arg, void (*hook)(void *arg, RTPFrame::Frame *));

    uint8_t *getInPacketBuffer() const;
    uint32_t getInPacketBufferLength() const;

    void addOutgoingFrame(RTPFrame::Frame *frame);

private:
    static int frameReceiver(RTPReader *reader);

    // TODO implement ring buffer
    bool active_;

    // connection-related stuff
    std::string srcAddr_;
    int srcPort_;

    // receiver thread related stuff
    std::thread *runner_;
    uint8_t *inPacketBuffer_; /* Buffer for incoming packet (MAX_PACKET) */
    uint32_t inPacketBufferLen_;

    std::vector<RTPFrame::Frame *>  framesOut_;
    std::mutex framesMtx_;

    // TODO
    void *receiveHookArg_;
    void (*receiveHook_)(void *arg, RTPFrame::Frame *frame);
};
