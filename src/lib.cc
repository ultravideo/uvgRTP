#include <cstdlib>
#include <iostream>

#include "debug.hh"
#include "conn.hh"
#include "hostname.hh"
#include "lib.hh"
#include "random.hh"

thread_local rtp_error_t rtp_errno;

kvz_rtp::context::context()
{
    cname_ = kvz_rtp::context::generate_cname();

#ifdef _WIN32
    WSADATA wsd;
    int rc;

    if ((rc = WSAStartup(MAKEWORD(2, 2), &wsd)) != 0) {
        win_get_last_error();
    }
#endif

    kvz_rtp::random::init();
}

kvz_rtp::context::~context()
{
    for (auto& conn : conns_) {
        delete conn.second;
    }

    conns_.clear();

#ifdef _WIN32
    WSACleanup();
#endif
}

kvz_rtp::reader *kvz_rtp::context::create_reader(std::string srcAddr, int srcPort, rtp_format_t fmt)
{
    kvz_rtp::reader *reader = new kvz_rtp::reader(fmt, srcAddr, srcPort);

    if (!reader) {
        std::cerr << "Failed to create kvz_rtp::reader for " << srcAddr << ":" << srcPort << "!" << std::endl;
        return nullptr;
    }

    conns_.insert(std::pair<int, connection *>(reader->get_ssrc(), reader));

    return reader;
}

kvz_rtp::writer *kvz_rtp::context::create_writer(std::string dstAddr, int dstPort, rtp_format_t fmt)
{
    kvz_rtp::writer *writer = new kvz_rtp::writer(fmt, dstAddr, dstPort);

    if (!writer) {
        LOG_ERROR("Failed to create writer for %s:%d!", dstAddr.c_str(), dstPort);
        return nullptr;
    }

    conns_.insert(std::pair<int, connection *>(writer->get_ssrc(), writer));

    return writer;
}

kvz_rtp::writer *kvz_rtp::context::create_writer(std::string dstAddr, int dstPort, int srcPort, rtp_format_t fmt)
{
    kvz_rtp::writer *writer = new kvz_rtp::writer(fmt, dstAddr, dstPort, srcPort);

    if (!writer) {
        LOG_ERROR("Failed to create writer for %s:%d!", dstAddr.c_str(), dstPort);
        return nullptr;
    }

    conns_.insert(std::pair<int, connection *>(writer->get_ssrc(), writer));

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

std::string kvz_rtp::context::generate_cname()
{
    std::string host = kvz_rtp::hostname::get_hostname();
    std::string user = kvz_rtp::hostname::get_username();

    if (host == "")
        host = generate_string(10);

    if (user == "")
        user = generate_string(10);

    return host + "@" + user;
}

std::string& kvz_rtp::context::get_cname()
{
    return cname_;
}
