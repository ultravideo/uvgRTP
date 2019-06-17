#include <iostream>

#include "debug.hh"
#include "conn.hh"
#include "lib.hh"

thread_local rtp_error_t rtp_errno;

kvz_rtp::context::context()
{
#ifdef _WIN32
    WSADATA wsd;
    int rc;

    if ((rc = WSAStartup(MAKEWORD(2, 2), &wsd)) != 0) {
        LOG_ERROR("Unable to load Winsock: %d\n", rc);
        /* TODO: how to stop everything?? */
    }
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

rtp_error_t kvz_rtp::context::destroy_writer(kvz_rtp::writer *writer)
{
    conns_.erase(writer->get_ssrc());

    delete writer;
    return RTP_OK;
}

rtp_error_t kvz_rtp::context::destroy_reader(kvz_rtp::reader *reader)
{
    conns_.erase(reader->get_ssrc());

    delete reader;
    return RTP_OK;
}
