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

class RTPConnection {

public:
    RTPConnection(bool reader);
    virtual ~RTPConnection();

    virtual int start() = 0;

    // getters
    uint32_t getId() const;
    uint16_t getSequence() const;
    uint32_t getTimestamp() const;
    uint32_t getSSRC() const;
    uint8_t getPayloadType() const;
    int getSocket() const;
    uint8_t *getInPacketBuffer() const;
    uint32_t getInPacketBufferLength() const;

    void setPayloadType(rtp_format_t fmt);

    void incRTPSequence(uint16_t seq);
    void incProcessedBytes(uint32_t nbytes);
    void incOverheadBytes(uint32_t nbytes);
    void incTotalBytes(uint32_t nbytes);
    void incProcessedPackets(uint32_t npackets);

    void fillFrame(RTPGeneric::GenericFrame *frame);

    void setConfig(void *config);
    void *getConfig();

protected:
    void *config_;
    uint32_t id_;
    int socket_;

private:
    bool reader_;

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
};
