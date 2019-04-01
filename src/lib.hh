#pragma once

#include <map>
#include "conn.hh"
#include "reader.hh"
#include "writer.hh"

class RTPContext {

public:
    RTPContext();
    ~RTPContext();

    /* Start listening to incoming RTP packets form srcAddr:srcPort
     *
     * Read packets are stored in a ring buffer which can be read by
     * calling RTPReader::pullFrame() */
    RTPReader *createReader(std::string srcAddr, int srcPort);
    RTPReader *createReader(std::string srcAddr, int srcPort, rtp_format_t fmt);

    /* Open connection for writing RTP packets to dstAddr:dstPort 
     *
     * Packets can be sent by calling RTPWriter::pushFrame() */
    RTPWriter *createWriter(std::string dstAddr, int dstPort);
    RTPWriter *createWriter(std::string dstAddr, int dstPort, rtp_format_t fmt);

    RTPConnection *openConnection(std::string dstAddr, int dstPort, int srcPort);
    int closeConnection(int id);

private:
    std::map<uint32_t, RTPConnection *> conns_;
};
