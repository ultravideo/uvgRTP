#include <iostream>

#include "lib.hh"
#include "conn.hh"

kvz_rtp::context::context()
{
#ifdef _WIN32
    // TODO initialize networking for windows
#endif
}

kvz_rtp::context::~context()
{
    for (auto it = conns_.begin(); it != conns_.end(); ++it) {
        delete it->second;
        conns_.erase(it);
    }
}

kvz_rtp::reader *kvz_rtp::context::create_reader(std::string srcAddr, int srcPort)
{
    kvz_rtp::reader *reader = new kvz_rtp::reader(srcAddr, srcPort);

    if (!reader) {
        std::cerr << "Failed to create kvz_rtp::reader for " << srcAddr << ":" << srcPort << "!" << std::endl;
        return nullptr;
    }

    conns_.insert(std::pair<int, connection *>(reader->get_ssrc(), reader));
    return reader;
}

kvz_rtp::reader *kvz_rtp::context::create_reader(std::string srcAddr, int srcPort, rtp_format_t fmt)
{
    kvz_rtp::reader *reader = nullptr;

    if ((reader = create_reader(srcAddr, srcPort)) == nullptr)
        return nullptr;

    reader->set_payload(fmt);
    return reader;
}

kvz_rtp::writer *kvz_rtp::context::create_writer(std::string dstAddr, int dstPort)
{
    kvz_rtp::writer *writer = new kvz_rtp::writer(dstAddr, dstPort);

    if (!writer) {
        std::cerr << "Failed to create writer for " << dstAddr << ":" << dstPort << "!" << std::endl;
        return nullptr;
    }

    conns_.insert(std::pair<int, connection *>(writer->get_ssrc(), writer));
    return writer;
}

kvz_rtp::writer *kvz_rtp::context::create_writer(std::string dstAddr, int dstPort, rtp_format_t fmt)
{
    kvz_rtp::writer *writer = nullptr;

    if ((writer = create_writer(dstAddr, dstPort)) == nullptr)
        return nullptr;

    conns_.insert(std::pair<int, connection *>(writer->get_ssrc(), writer));
    writer->set_payload(fmt);
    return writer;
}

kvz_rtp::writer *kvz_rtp::context::create_writer(std::string dstAddr, int dstPort, int srcPort)
{
    kvz_rtp::writer *writer = new kvz_rtp::writer(dstAddr, dstPort, srcPort);

    if (!writer) {
        std::cerr << "Failed to create writer for " << dstAddr << ":" << dstPort << "!" << std::endl;
        return nullptr;
    }

    conns_.insert(std::pair<int, connection *>(writer->get_ssrc(), writer));
    return writer;
}

kvz_rtp::writer *kvz_rtp::context::create_writer(std::string dstAddr, int dstPort, int srcPort, rtp_format_t fmt)
{
    kvz_rtp::writer *writer = nullptr;

    if ((writer = create_writer(dstAddr, dstPort, srcPort)) == nullptr)
        return nullptr;

    conns_.insert(std::pair<int, connection *>(writer->get_ssrc(), writer));
    writer->set_payload(fmt);
    return writer;
}
