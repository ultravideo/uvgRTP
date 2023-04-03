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

uvgrtp::socketfactory::socketfactory() :
    local_address_(""),
    local_port_(),
    ipv6_(false),
    socket_(std::shared_ptr<uvgrtp::socket>(new uvgrtp::socket(RCE_NO_FLAGS)))
{}

uvgrtp::socketfactory::~socketfactory()
{}

rtp_error_t uvgrtp::socketfactory::set_local_interface(std::string local_addr, uint16_t local_port)
{
    rtp_error_t ret = RTP_GENERIC_ERROR;

    local_address_ = local_addr;
    local_port_ = local_port;

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

    if (::ioctlsocket(socket_->get_raw_socket(), FIONBIO, (u_long*)&enabled) < 0)
#endif

}