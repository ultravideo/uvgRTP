#include <iostream>

#include "lib.hh"
#include "conn.hh"

RTPContext::RTPContext()
{
#ifdef _WIN32
    // TODO initialize networking for windows
#endif
}

RTPContext::~RTPContext()
{
    for (auto it = conns_.begin(); it != conns_.end(); ++it) {
        delete it->second;
        conns_.erase(it);
    }
}

RTPReader *RTPContext::createReader(std::string srcAddr, int srcPort)
{
    RTPReader *reader = new RTPReader(srcAddr, srcPort);

    if (!reader) {
        std::cerr << "Failed to create RTPReader for " << srcAddr << ":" << srcPort << "!" << std::endl;
        return nullptr;
    }

    conns_.insert(std::pair<int, RTPConnection *>(reader->getId(), reader));
    return reader;
}

RTPReader *RTPContext::createReader(std::string srcAddr, int srcPort, rtp_format_t fmt)
{
    RTPReader *reader = nullptr;

    if ((reader = createReader(srcAddr, srcPort)) == nullptr)
        return nullptr;

    reader->setPayloadType(fmt);
    return reader;
}

RTPWriter *RTPContext::createWriter(std::string dstAddr, int dstPort)
{
    RTPWriter *writer = new RTPWriter(dstAddr, dstPort);

    if (!writer) {
        std::cerr << "Failed to create RTPWriter for " << dstAddr << ":" << dstPort << "!" << std::endl;
        return nullptr;
    }

    conns_.insert(std::pair<int, RTPConnection *>(writer->getId(), writer));
    return writer;
}

RTPWriter *RTPContext::createWriter(std::string dstAddr, int dstPort, rtp_format_t fmt)
{
    RTPWriter *writer = nullptr;

    if ((writer = createWriter(dstAddr, dstPort)) == nullptr)
        return nullptr;

    conns_.insert(std::pair<int, RTPConnection *>(writer->getId(), writer));
    writer->setPayloadType(fmt);
    return writer;
}

RTPWriter *RTPContext::createWriter(std::string dstAddr, int dstPort, int srcPort)
{
    RTPWriter *writer = new RTPWriter(dstAddr, dstPort, srcPort);

    if (!writer) {
        std::cerr << "Failed to create RTPWriter for " << dstAddr << ":" << dstPort << "!" << std::endl;
        return nullptr;
    }

    conns_.insert(std::pair<int, RTPConnection *>(writer->getId(), writer));
    return writer;
}

RTPWriter *RTPContext::createWriter(std::string dstAddr, int dstPort, int srcPort, rtp_format_t fmt)
{
    RTPWriter *writer = nullptr;

    if ((writer = createWriter(dstAddr, dstPort, srcPort)) == nullptr)
        return nullptr;

    conns_.insert(std::pair<int, RTPConnection *>(writer->getId(), writer));
    writer->setPayloadType(fmt);
    return writer;
}
