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
    used_ports_({}),
    ipv6_(false),
    used_sockets_({}),
    local_bound_(false)
{}

uvgrtp::socketfactory::~socketfactory()
{}

rtp_error_t uvgrtp::socketfactory::set_local_interface(std::string local_addr)
{
    rtp_error_t ret = RTP_GENERIC_ERROR;

    local_address_ = local_addr;
    // check IP address family
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
    return RTP_OK;
}

rtp_error_t uvgrtp::socketfactory::create_new_socket(uint16_t local_port)
{
    rtp_error_t ret = RTP_OK;
    if (std::find(used_ports_.begin(), used_ports_.end(), local_port) == used_ports_.end()) {
        std::shared_ptr<uvgrtp::socket> socket = std::make_shared<uvgrtp::socket>(new uvgrtp::socket(rce_flags_));

        if (ipv6_) {
            if ((ret = socket->init(AF_INET6, SOCK_DGRAM, 0)) != RTP_OK) {
                return ret;
            }
        }
        else {
            if ((ret = socket->init(AF_INET6, SOCK_DGRAM, 0)) != RTP_OK) {
                return ret;
            }
        }
#ifdef _WIN32
        /* Make the socket non-blocking */
        int enabled = 1;

        if (::ioctlsocket(socket->get_raw_socket(), FIONBIO, (u_long*)&enabled) < 0) {
            return RTP_GENERIC_ERROR;
        }
#endif
        used_sockets_.push_back(socket);


        if (ipv6_) {
            sockaddr_in6 bind_addr6 = socket->create_ip6_sockaddr(local_address_, local_port);
            ret = socket->bind_ip6(bind_addr6);
        }
        else {
            sockaddr_in bind_addr = socket->create_sockaddr(AF_INET, local_address_, local_port);
            ret = socket->bind(bind_addr);
        }
        if (ret == RTP_OK) {
            used_ports_.push_back(local_port);
        }
    }
    
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