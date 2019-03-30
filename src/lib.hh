#pragma once

#include <map>
#include "conn.hh"

class RTPContext {

public:
    RTPContext();
    ~RTPContext();

    RTPConnection *openConnection(std::string dstAddr, int dstPort, int srcPort);
    int closeConnection(int id);

private:
    std::map<uint32_t, RTPConnection *> conns_;
};
