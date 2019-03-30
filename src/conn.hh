#pragma once

#ifdef _WIN32
#include <inaddr.h>
#else
#include <netinet/ip.h>
#endif

#include <string>
#include <vector>
#include <thread>
#include <mutex>

#include "util.hh"
#include "rtp_generic.hh"

class RTPPayload {

};

class RTPConnection {

public:
    RTPConnection(std::string dstAddr, int dstPort, int srcPort);
    ~RTPConnection();

    // TODO
    int open();

    // getters
    uint32_t getId() const;
    uint16_t getSequence() const;
    uint32_t getTimestamp() const;
    uint32_t getSSRC() const;
    uint8_t getPayload() const;
    int getSocket() const;
    sockaddr_in getOutAddress() const;
    uint8_t *getInPacketBuffer() const;
    uint32_t getInPacketBufferLength() const;

    void incRTPSequence(uint16_t seq);
    void incProcessedBytes(uint32_t nbytes);
    void incOverheadBytes(uint32_t nbytes);
    void incTotalBytes(uint32_t nbytes);
    void incProcessedPackets(uint32_t npackets);

    int pushFrame(uint8_t *data, uint32_t datalen, rtp_format_t fmt, uint32_t timestamp);
    RTPGeneric::GenericFrame *pullFrame();

    void fillFrame(RTPGeneric::GenericFrame *frame);

    void addOutgoingFrame(RTPGeneric::GenericFrame *frame);

private:
    std::string dstAddr_;
    int dstPort_;
    int srcPort_;

    uint32_t id_;
    int socket_;

    sockaddr_in addrIn_;
    sockaddr_in addrOut_;

    /* fRTPFormat inFormat; */
    /* fRTPFormat outFormat; */

    /* Receiving */
    std::thread *runner_;
    uint8_t *inPacketBuffer_; /* Buffer for incoming packet (MAX_PACKET) */
    uint32_t inPacketBufferLen_;

    uint8_t *frameBuffer_; /* Larger buffer for storing incoming frame */
    uint32_t frameBufferLen_;

    std::vector<RTPGeneric::GenericFrame *>  framesOut_;
    std::mutex framesMtx_;

    // RTP
    uint16_t rtp_sequence_;
    uint8_t  rtp_payload_;
    uint32_t rtp_timestamp_;
    uint32_t rtp_ssrc_;

    // Statistics
    uint32_t processedBytes_;
    uint32_t overheadBytes_;
    uint32_t totalBytes_;
    uint32_t processedPackets_;

    uint8_t *config;
};
