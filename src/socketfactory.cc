#include "socketfactory.hh"

uvgrtp::socketfactory::socketfactory() : 
    local_address_(""),
    local_port_()
{}

uvgrtp::socketfactory::~socketfactory()
{}

void uvgrtp::socketfactory::set_local_interface(std::string local_addr, uint16_t local_port)
{
    local_address_ = local_addr;
    local_port_ = local_port;
}