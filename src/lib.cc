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

    std::terminate();
}

RTPConnection *RTPContext::openConnection(std::string dstAddr, int dstPort, int srcPort)
{
    RTPConnection *conn = new RTPConnection(dstAddr, dstPort, srcPort);

    if (!conn) {
        std::cerr << "Failed to create RTP connection!" << std::endl;
        return nullptr;
    }

    if (conn->open() < 0) {
        std::cerr << "Failed to open RTP connection!" << std::endl;
        delete conn;
        return nullptr;
    }

    conns_.insert(std::pair<int, RTPConnection *>(conn->getId(), conn));
    return conn;
}

int RTPContext::closeConnection(int id)
{
    std::map<uint32_t, RTPConnection *>::iterator it;

    if ((it = conns_.find(id)) == conns_.end()) {
        std::cerr << "Connection with id " << id << " does not exist!" << std::endl;
        return -1;
    }

    delete it->second;
    conns_.erase(it);

    return 0;
}
