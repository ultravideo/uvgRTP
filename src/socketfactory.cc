#include "socketfactory.hh"
#include "socket.hh"
#ifdef _WIN32
#include <Ws2tcpip.h>
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#endif
#include <algorithm>

uvgrtp::socketfactory::socketfactory(int rce_flags) :
    rce_flags_(rce_flags),
    local_address_(""),
    local_port_(),
    ipv6_(false),
    socket_(std::shared_ptr<uvgrtp::socket>(new uvgrtp::socket(rce_flags))),
    local_bound_(false),
    used_ports_({})
{}

uvgrtp::socketfactory::~socketfactory()
{}

rtp_error_t uvgrtp::socketfactory::set_local_interface(std::string local_addr)
{
    rtp_error_t ret = RTP_GENERIC_ERROR;

    local_address_ = local_addr;
    // check IP address family and initialize the socket
    struct addrinfo hint, * res = NULL;
    memset(&hint, '\0', sizeof(hint));
    hint.ai_family = PF_UNSPEC;
    hint.ai_flags = AI_NUMERICHOST;

    if (getaddrinfo(local_address_.c_str(), NULL, &hint, &res) != 0) {
        return RTP_GENERIC_ERROR;
    }
    if (res->ai_family == AF_INET6) {
        ipv6_ = true;

    }
    if ((ret = socket_->init(res->ai_family, SOCK_DGRAM, 0)) != RTP_OK) {
        return ret;
    }

#ifdef _WIN32
    /* Make the socket non-blocking */
    int enabled = 1;

    if (::ioctlsocket(socket_->get_raw_socket(), FIONBIO, (u_long*)&enabled) < 0) {
        return RTP_GENERIC_ERROR;
    }
#endif

    return RTP_OK;
}

rtp_error_t uvgrtp::socketfactory::bind_local_socket(uint16_t local_port)
{
    rtp_error_t ret = RTP_OK;
    
    if (std::find(used_ports_.begin(), used_ports_.end(), local_port) == used_ports_.end()) {
        if (ipv6_) {
            sockaddr_in6 bind_addr6 = socket_->create_ip6_sockaddr(local_address_, local_port);
            ret = socket_->bind_ip6(bind_addr6);
        }
        else {
            sockaddr_in bind_addr = socket_->create_sockaddr(AF_INET, local_address_, local_port);
            ret = socket_->bind(bind_addr);
        }
        if (ret == RTP_OK) {
            used_ports_.push_back(local_port);
            local_bound_ = true;
        }
    }
    // = local_port;
    return ret;
}

bool uvgrtp::socketfactory::get_local_bound() const
{
    return local_bound_;
}

std::shared_ptr<uvgrtp::socket> uvgrtp::socketfactory::get_socket_ptr() const
{
    return socket_;
}