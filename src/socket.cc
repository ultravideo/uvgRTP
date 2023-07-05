#include "socket.hh"

#include "uvgrtp/util.hh"

#include "debug.hh"
#include "memory.hh"

#include <thread>

#ifdef _WIN32
#include <Windows.h>
#include <winsock2.h>
#include <Ws2tcpip.h>
#include <ws2def.h>
#include <ws2ipdef.h>
#else
#include <unistd.h>
#include <poll.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <netdb.h>
#endif

#if defined(__MINGW32__) || defined(__MINGW64__)
#include "mingw_inet.hh"
using namespace uvgrtp;
using namespace mingw;
#endif

#include <cstring>
#include <cassert>


#define WSABUF_SIZE 256

uvgrtp::socket::socket(int rce_flags) :
    socket_(0),
    local_address_(),
    local_ip6_address_(),
    ipv6_(false),
    rce_flags_(rce_flags),
#ifdef _WIN32
    buffers_()
#else
    header_(),
    chunks_()
#endif
{}

uvgrtp::socket::~socket()
{
    UVG_LOG_DEBUG("Socket total sent packets is %lu and received packets is %lu", sent_packets_, received_packets_);

#ifndef _WIN32
    close(socket_);
#else
    closesocket(socket_);
#endif
}

rtp_error_t uvgrtp::socket::init(short family, int type, int protocol)
{
    if (family == AF_INET6) {
        ipv6_ = true;
    }
    else {
       ipv6_ = false;
    }

#ifdef _WIN32
    if ((socket_ = ::socket(family, type, protocol)) == INVALID_SOCKET) {
        win_get_last_error();
#else
    if ((socket_ = ::socket(family, type, protocol)) < 0) {
        UVG_LOG_ERROR("Failed to create socket: %s", strerror(errno));
#endif
        return RTP_SOCKET_ERROR;
    }

#ifdef _WIN32
    BOOL bNewBehavior     = FALSE;
    DWORD dwBytesReturned = 0;

    WSAIoctl(socket_, _WSAIOW(IOC_VENDOR, 12), &bNewBehavior, sizeof(bNewBehavior), NULL, 0, &dwBytesReturned, NULL, NULL);
#endif

    return RTP_OK;
}

rtp_error_t uvgrtp::socket::setsockopt(int level, int optname, const void *optval, socklen_t optlen)
{
    std::lock_guard<std::mutex> lg(conf_mutex_);
    if (::setsockopt(socket_, level, optname, (const char *)optval, optlen) < 0) {

        //strerror(errno), depricated
        UVG_LOG_ERROR("Failed to set socket options");
        return RTP_GENERIC_ERROR;
    }

    return RTP_OK;
}

rtp_error_t uvgrtp::socket::bind(short family, unsigned host, short port)
{
    assert(family == AF_INET);
    local_address_ = create_sockaddr(family, host, port);
    return bind(local_address_);
}

bool uvgrtp::socket::is_multicast(sockaddr_in& local_address)
{
    // Multicast addresses ranges from 224.0.0.0 to 239.255.255.255 (0xE0000000 to 0xEFFFFFFF)
    auto addr = local_address.sin_addr.s_addr;
    return (ntohl(addr) & 0xF0000000) == 0xE0000000;
}

rtp_error_t uvgrtp::socket::bind(sockaddr_in& local_address)
{
    local_address_ = local_address;

    UVG_LOG_DEBUG("Binding to address %s", sockaddr_to_string(local_address_).c_str());

    if (!uvgrtp::socket::is_multicast(local_address_)) {
        // Regular address
        if (::bind(socket_, (struct sockaddr*)&local_address_, sizeof(local_address_)) < 0) {
#ifdef _WIN32
            win_get_last_error();
#else
            fprintf(stderr, "%s\n", strerror(errno));
#endif
            UVG_LOG_ERROR("Binding to port %u failed!", ntohs(local_address_.sin_port));
            return RTP_BIND_ERROR;
        }
    } else {
        // Multicast address
        // Reuse address to enabled receiving the same stream multiple times
        const int enable = 1;
        if (::setsockopt(socket_, SOL_SOCKET, SO_REUSEADDR, (const char*)&enable, sizeof(int)) < 0) {
#ifdef _WIN32
            win_get_last_error();
#else
            fprintf(stderr, "%s\n", strerror(errno));
#endif

            UVG_LOG_ERROR("Reuse address failed!");
        }

        // Bind with empty address
        auto bind_addr_in = local_address_;
        bind_addr_in.sin_addr.s_addr = htonl(INADDR_ANY);

        if (::bind(socket_, (struct sockaddr*)&bind_addr_in, sizeof(bind_addr_in)) < 0) {
#ifdef _WIN32
            win_get_last_error();
#else
            fprintf(stderr, "%s\n", strerror(errno));
#endif
            UVG_LOG_ERROR("Binding to port %u failed!", ntohs(bind_addr_in.sin_port));
            return RTP_BIND_ERROR;
        }

        // Join multicast membership
        struct ip_mreq mreq{};
        mreq.imr_multiaddr.s_addr = local_address_.sin_addr.s_addr;
        mreq.imr_interface.s_addr = htonl(INADDR_ANY);

        if (::setsockopt(socket_, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char*)&mreq, sizeof(mreq)) < 0) {
#ifdef _WIN32
            win_get_last_error();
#else
            fprintf(stderr, "%s\n", strerror(errno));
#endif
            UVG_LOG_ERROR("Multicast join failed!");
            return RTP_BIND_ERROR;
        }
    }

    return RTP_OK;
}

bool uvgrtp::socket::is_multicast(sockaddr_in6& local_address)
{
    // Multicast IP addresses have their first byte equals to 0xFF
    auto addr = local_address.sin6_addr.s6_addr;
    return addr[0] == 0xFF;
}

rtp_error_t uvgrtp::socket::bind_ip6(sockaddr_in6& local_address)
{
    local_ip6_address_ = local_address;
    UVG_LOG_DEBUG("Binding to address %s", sockaddr_ip6_to_string(local_ip6_address_).c_str());

    if (!uvgrtp::socket::is_multicast(local_ip6_address_)) {
        if (::bind(socket_, (struct sockaddr*)&local_ip6_address_, sizeof(local_ip6_address_)) < 0) {
    #ifdef _WIN32
            win_get_last_error();
    #else
            fprintf(stderr, "%s\n", strerror(errno));
    #endif
            UVG_LOG_ERROR("Binding to port %u failed!", ntohs(local_ip6_address_.sin6_port));
            return RTP_BIND_ERROR;
        }
    } else {
        // Multicast address
        // Reuse address to enabled receiving the same stream multiple times
        const int enable = 1;
        if (::setsockopt(socket_, SOL_SOCKET, SO_REUSEADDR, (const char*)&enable, sizeof(int)) < 0) {
#ifdef _WIN32
            win_get_last_error();
#else
            fprintf(stderr, "%s\n", strerror(errno));
#endif

            UVG_LOG_ERROR("Reuse address failed!");
        }

        // Bind with empty address
        auto bind_addr_in = local_ip6_address_;
        bind_addr_in.sin6_addr = in6addr_any;

        if (::bind(socket_, (struct sockaddr*)&bind_addr_in, sizeof(bind_addr_in)) < 0) {
#ifdef _WIN32
            win_get_last_error();
#else
            fprintf(stderr, "%s\n", strerror(errno));
#endif
            UVG_LOG_ERROR("Binding to port %u failed!", ntohs(bind_addr_in.sin6_port));
            return RTP_BIND_ERROR;
        }

        // Join multicast membership
        struct ipv6_mreq mreq{};
        memcpy(&mreq.ipv6mr_multiaddr, &local_ip6_address_.sin6_addr, sizeof(mreq.ipv6mr_multiaddr));

        if (::setsockopt(socket_, IPPROTO_IPV6, IPV6_JOIN_GROUP, (char*)&mreq, sizeof(mreq)) < 0) {
#ifdef _WIN32
            win_get_last_error();
#else
            fprintf(stderr, "%s\n", strerror(errno));
#endif
            UVG_LOG_ERROR("Multicast join failed!");
            return RTP_BIND_ERROR;
        }
    }

    return RTP_OK;
}

int uvgrtp::socket::check_family(std::string addr) 
{
    // Use getaddrinfo() to determine whether we are using ipv4 or ipv6 addresses
    struct addrinfo hint, * res = NULL;
    memset(&hint, '\0', sizeof(hint));
    hint.ai_family = PF_UNSPEC;
    hint.ai_flags = AI_NUMERICHOST;

    if (getaddrinfo(addr.c_str(), NULL, &hint, &res) != 0) {
        UVG_LOG_ERROR("Invalid IP address");
        return RTP_GENERIC_ERROR;
    }
    if (res->ai_family == AF_INET6) {
        UVG_LOG_DEBUG("Using an IPv6 address");
        return 2;
    }
    else {
        UVG_LOG_DEBUG("Using an IPv4 address");
        return 1;
    }
    return -1;
}

sockaddr_in uvgrtp::socket::create_sockaddr(short family, unsigned host, short port)
{
    assert(family == AF_INET);

    sockaddr_in addr;

    memset(&addr, 0, sizeof(addr));

    addr.sin_family      = family;
    addr.sin_port        = htons(port);
    addr.sin_addr.s_addr = htonl(host);

    return addr;
}

sockaddr_in uvgrtp::socket::create_sockaddr(short family, std::string host, short port)
{
    assert(family == AF_INET);

    sockaddr_in addr;

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = family;

    inet_pton(AF_INET, host.c_str(), &addr.sin_addr);
    addr.sin_port = htons((uint16_t)port);

    return addr;
}

// This function seems to not be currently used anywhere
sockaddr_in6 uvgrtp::socket::create_ip6_sockaddr(unsigned host, short port)
{

    sockaddr_in6 addr;
    memset(&addr, 0, sizeof(addr));

    addr.sin6_family = AF_INET6;
    std::string host_str = std::to_string(host);
    inet_pton(AF_INET6, host_str.c_str(), &addr.sin6_addr);
    addr.sin6_port = htons((uint16_t)port);

    return addr;
}

sockaddr_in6 uvgrtp::socket::create_ip6_sockaddr(std::string host, short port)
{
    sockaddr_in6 addr;
    memset(&addr, 0, sizeof(addr));

    addr.sin6_family = AF_INET6;
    inet_pton(AF_INET6, host.c_str(), &addr.sin6_addr);
    addr.sin6_port = htons((uint16_t)port);

    return addr;
}

sockaddr_in6 uvgrtp::socket::create_ip6_sockaddr_any(short src_port) {
    sockaddr_in6 addr;
    memset(&addr, 0, sizeof(addr));

    addr.sin6_family = AF_INET6;
    addr.sin6_addr = in6addr_any;
    addr.sin6_port = htons(src_port);

    return addr;
}

std::string uvgrtp::socket::get_socket_path_string() const
{   
    /*if (ipv6_) {
        return sockaddr_ip6_to_string(local_ip6_address_) + " -> " + sockaddr_ip6_to_string(remote_ip6_address_);
    }
    return sockaddr_to_string(local_address_) + " -> " + sockaddr_to_string(remote_address_);*/
    return "Not implemented";
}

std::string uvgrtp::socket::sockaddr_to_string(const sockaddr_in& addr)
{
    int addr_len = INET_ADDRSTRLEN;
    char* c_string = new char[INET_ADDRSTRLEN];
    memset(c_string, 0, INET_ADDRSTRLEN);

    char* addr_string = new char[addr_len];
    memset(addr_string, 0, addr_len);

#ifdef WIN32
    PVOID pvoid_sin_addr = const_cast<PVOID>((void*)(&addr.sin_addr));
    inet_ntop(addr.sin_family, pvoid_sin_addr, addr_string, addr_len);
#else
    inet_ntop(addr.sin_family, &addr.sin_addr, addr_string, addr_len);
#endif

    std::string string(addr_string);
    string.append(":" + std::to_string(ntohs(addr.sin_port)));

    delete[] addr_string;
    return string;
}

std::string uvgrtp::socket::sockaddr_ip6_to_string(const sockaddr_in6& addr6)
{
    char* c_string = new char[INET6_ADDRSTRLEN];
    memset(c_string, 0, INET6_ADDRSTRLEN);
    inet_ntop(AF_INET6, &addr6.sin6_addr, c_string, INET6_ADDRSTRLEN);
    std::string string(c_string);
    string.append(":" + std::to_string(ntohs(addr6.sin6_port)));
    delete[] c_string;
    return string;
}

socket_t& uvgrtp::socket::get_raw_socket()
{
    return socket_;
}

rtp_error_t uvgrtp::socket::install_handler(std::shared_ptr<std::atomic<std::uint32_t>> local_ssrc, void* arg, packet_handler_vec handler)
{
    handlers_mutex_.lock();
    if (!handler)
        return RTP_INVALID_VALUE;


    socket_packet_handler hndlr;
    hndlr.arg = arg;
    hndlr.handler = handler;
    vec_handlers_.insert({local_ssrc, hndlr});
    handlers_mutex_.unlock();

    return RTP_OK;
}

rtp_error_t uvgrtp::socket::remove_handler(std::shared_ptr<std::atomic<std::uint32_t>> local_ssrc)

{
    handlers_mutex_.lock();
    vec_handlers_.erase(local_ssrc);
    handlers_mutex_.unlock();
    return RTP_OK;
}

rtp_error_t uvgrtp::socket::__sendto(sockaddr_in& addr, sockaddr_in6& addr6, bool ipv6, uint8_t *buf, size_t buf_len, int send_flags, int *bytes_sent)
{
    int nsend = 0;

#ifndef _WIN32
    if (ipv6) {
        nsend = ::sendto(socket_, buf, buf_len, send_flags, (const struct sockaddr*)&addr6, sizeof(addr6));
    }
    else {
        nsend = ::sendto(socket_, buf, buf_len, send_flags, (const struct sockaddr*)&addr, sizeof(addr));
    }
    if (nsend == -1) {
        UVG_LOG_ERROR("Failed to send data: %s", strerror(errno));

        if (bytes_sent) {
            *bytes_sent = -1;
        }
        return RTP_SEND_ERROR;
    }
#else
    DWORD sent_bytes = 0;
    WSABUF data_buf;

    data_buf.buf = (char *)buf;
    data_buf.len = (ULONG)buf_len;
    int result = -1;
    if (ipv6) {
        result = WSASendTo(socket_, &data_buf, 1, &sent_bytes, send_flags, (const struct sockaddr*)&addr6, sizeof(addr6), nullptr, nullptr);
    }
    else {
        result = WSASendTo(socket_, &data_buf, 1, &sent_bytes, send_flags, (const struct sockaddr*)&addr, sizeof(addr), nullptr, nullptr);
    }
    if (result == -1) {
        win_get_last_error();
        if (ipv6_) {
            UVG_LOG_ERROR("Failed to send to %s", sockaddr_ip6_to_string(addr6).c_str());
        }
        else {
            UVG_LOG_ERROR("Failed to send to %s", sockaddr_to_string(addr).c_str());
        }

        if (bytes_sent)
            *bytes_sent = -1;
        return RTP_SEND_ERROR;
    }
    nsend = sent_bytes;
#endif

    if (bytes_sent) {
        *bytes_sent = nsend;
    }

#ifndef NDEBUG
    ++sent_packets_;
#endif // !NDEBUG

    return RTP_OK;
}

rtp_error_t uvgrtp::socket::sendto(sockaddr_in& addr, sockaddr_in6& addr6, uint8_t *buf, size_t buf_len, int send_flags, int *bytes_sent)
{
    return __sendto(addr, addr6, ipv6_, buf, buf_len, send_flags, bytes_sent);
}

rtp_error_t uvgrtp::socket::sendto(sockaddr_in& addr, sockaddr_in6& addr6, uint8_t *buf, size_t buf_len, int send_flags)
{
    return __sendto(addr, addr6, ipv6_, buf, buf_len, send_flags, nullptr);
}

rtp_error_t uvgrtp::socket::__sendtov(
    sockaddr_in& addr,
    sockaddr_in6& addr6,
    bool ipv6,
    uvgrtp::buf_vec& buffers,
    int send_flags, int *bytes_sent
)
{
#ifndef _WIN32
    int sent_bytes = 0;

    for (size_t i = 0; i < buffers.size(); ++i) {
        chunks_[i].iov_len  = buffers.at(i).first;
        chunks_[i].iov_base = buffers.at(i).second;

        sent_bytes += buffers.at(i).first;
    }
    if (ipv6) {
        header_.msg_hdr.msg_name = (void*)&addr6;
        header_.msg_hdr.msg_namelen = sizeof(addr6);
    }
    else {
        header_.msg_hdr.msg_name       = (void *)&addr;
        header_.msg_hdr.msg_namelen    = sizeof(addr);
    }
    header_.msg_hdr.msg_iov        = chunks_;
    header_.msg_hdr.msg_iovlen     = buffers.size();
    header_.msg_hdr.msg_control    = 0;
    header_.msg_hdr.msg_controllen = 0;

    if (sendmmsg(socket_, &header_, 1, send_flags) < 0) {
        UVG_LOG_ERROR("Failed to send RTP frame: %s!", strerror(errno));
        set_bytes(bytes_sent, -1);
        return RTP_SEND_ERROR;
    }
#else
    DWORD sent_bytes = 0;

    // DWORD corresponds to uint16 on most platforms
    if (buffers.size() > UINT16_MAX)
    {
        UVG_LOG_ERROR("Trying to send too large buffer");
        return RTP_INVALID_VALUE;
    }

    /* create WSABUFs from input buffers and send them at once */
    for (size_t i = 0; i < buffers.size(); ++i) {
        buffers_[i].len = (ULONG)buffers.at(i).first;
        buffers_[i].buf = (char *)buffers.at(i).second;
    }
    int success = 0;
    if (ipv6) {
        success = WSASendTo(socket_, buffers_, (DWORD)buffers.size(), &sent_bytes, send_flags,
            (SOCKADDR*)&addr6, sizeof(addr6), nullptr, nullptr);
    }
    else {
        success = WSASendTo(socket_, buffers_, (DWORD)buffers.size(), &sent_bytes, send_flags,
            (SOCKADDR*)&addr, sizeof(addr), nullptr, nullptr);
    }
    if (success != 0) {
        win_get_last_error();
        if (ipv6_) {
            UVG_LOG_ERROR("Failed to send to %s", sockaddr_ip6_to_string(addr6).c_str());
        }
        else {
            UVG_LOG_ERROR("Failed to send to %s", sockaddr_to_string(addr).c_str());
        }

        set_bytes(bytes_sent, -1);
        return RTP_SEND_ERROR;
    }


#endif

#ifndef NDEBUG
    ++sent_packets_;
#endif // !NDEBUG

    set_bytes(bytes_sent, sent_bytes);
    return RTP_OK;
}

rtp_error_t uvgrtp::socket::sendto(sockaddr_in& addr, sockaddr_in6& addr6, buf_vec& buffers, int send_flags)
{
    rtp_error_t ret = RTP_OK;

    for (auto& handler : vec_handlers_) {
        std::lock_guard<std::mutex> lg(handlers_mutex_);
        if ((ret = (*handler.second.handler)(handler.second.arg, buffers)) != RTP_OK) {
            UVG_LOG_ERROR("Malformed packet");
            return ret;
        }
    }
    // buf_vec
    return __sendtov(addr, addr6, ipv6_, buffers, send_flags, nullptr);
}

rtp_error_t uvgrtp::socket::sendto(
    sockaddr_in& addr,
    sockaddr_in6 & addr6,
    buf_vec& buffers,
    int send_flags, int *bytes_sent
)
{
    rtp_error_t ret = RTP_OK;

    for (auto& handler : vec_handlers_) {
        if ((ret = (*handler.second.handler)(handler.second.arg, buffers)) != RTP_OK) {
            UVG_LOG_ERROR("Malformed packet");
            return ret;
        }
    }
    // buf_vec
    return __sendtov(addr, addr6, ipv6_, buffers, send_flags, bytes_sent);
}

rtp_error_t uvgrtp::socket::__sendtov(
    sockaddr_in& addr,
    sockaddr_in6& addr6,
    bool ipv6,
    uvgrtp::pkt_vec& buffers,
    int send_flags, int *bytes_sent
)
{
    rtp_error_t return_value = RTP_OK;
    int sent_bytes = 0;

#ifndef _WIN32

    struct mmsghdr *headers = new struct mmsghdr[buffers.size()];
    struct mmsghdr *hptr = headers;

    for (size_t i = 0; i < buffers.size(); ++i) {
        headers[i].msg_hdr.msg_iov        = new struct iovec[buffers[i].size()];
        headers[i].msg_hdr.msg_iovlen     = buffers[i].size();
        if (ipv6) {
            headers[i].msg_hdr.msg_name = (void*)&addr6;
            headers[i].msg_hdr.msg_namelen = sizeof(addr6);
        }
        else {
            headers[i].msg_hdr.msg_name       = (void *)&addr;
            headers[i].msg_hdr.msg_namelen    = sizeof(addr);
        }
        headers[i].msg_hdr.msg_control    = 0;
        headers[i].msg_hdr.msg_controllen = 0;

        for (size_t k = 0; k < buffers[i].size(); ++k) {
            headers[i].msg_hdr.msg_iov[k].iov_len   = buffers[i][k].first;
            headers[i].msg_hdr.msg_iov[k].iov_base  = buffers[i][k].second;
            sent_bytes                             += buffers[i][k].first;
        }
    }

    ssize_t npkts = (rce_flags_ & RCE_SYSTEM_CALL_CLUSTERING) ? 1024 : 1;
    ssize_t bptr  = buffers.size();

    while (bptr > npkts) {
        if (sendmmsg(socket_, hptr, npkts, send_flags) < 0) {
            log_platform_error("sendmmsg(2) failed");
            return_value = RTP_SEND_ERROR;
            break;
        }

        bptr -= npkts;
        hptr += npkts;
    }

    if (return_value == RTP_OK)
    {
        if (sendmmsg(socket_, hptr, bptr, send_flags) < 0) {
            log_platform_error("sendmmsg(2) failed");
            return_value = RTP_SEND_ERROR;
        }
    }

    for (size_t i = 0; i < buffers.size(); ++i)
    {
        if (headers[i].msg_hdr.msg_iov)
        {
            delete[] headers[i].msg_hdr.msg_iov;
        }
    }
    delete[] headers;

#else
    INT ret = 0;
    WSABUF wsa_bufs[WSABUF_SIZE];

    for (auto& buffer : buffers) {

        if (buffer.size() > WSABUF_SIZE) {
            UVG_LOG_ERROR("Input vector to __sendtov() has more than %u elements!", WSABUF_SIZE);
            return_value = RTP_GENERIC_ERROR;
            break;
        }
        /* create WSABUFs from input buffer and send them at once */
        for (size_t i = 0; i < buffer.size(); ++i) {
            wsa_bufs[i].len = (ULONG)buffer.at(i).first;
            wsa_bufs[i].buf = (char *)buffer.at(i).second;
        }

send_:
        DWORD sent_bytes_dw = 0;
        if (ipv6) {
            ret = WSASendTo(
                socket_,
                wsa_bufs,
                (DWORD)buffer.size(),
                &sent_bytes_dw,
                send_flags,
                (SOCKADDR*)&addr6,
                sizeof(addr6),
                nullptr,
                nullptr
            );
        }
        else {
            ret = WSASendTo(
                socket_,
                wsa_bufs,
                (DWORD)buffer.size(),
                &sent_bytes_dw,
                send_flags,
                (SOCKADDR*)&addr,
                sizeof(addr),
                nullptr,
                nullptr
            );
        }
        sent_bytes = sent_bytes_dw;

        if (ret == SOCKET_ERROR) {

            int error = WSAGetLastError();
            if (error == WSAEWOULDBLOCK) {
                UVG_LOG_DEBUG("WSASendTo would block, trying again after 3 ms");
                std::this_thread::sleep_for(std::chrono::milliseconds(3));
                goto send_;
            }
            else
            {
                UVG_LOG_DEBUG("WSASendTo failed with error %li", error);
                log_platform_error("WSASendTo() failed");
                if (ipv6_) {
                    UVG_LOG_ERROR("Failed to send to %s", sockaddr_ip6_to_string(addr6).c_str());
                }
                else {
                    UVG_LOG_ERROR("Failed to send to %s", sockaddr_to_string(addr).c_str());
                }
            }

            sent_bytes = -1;
            return_value = RTP_SEND_ERROR;
            break;
        }
    }
#endif

#ifndef NDEBUG
    sent_packets_ += buffers.size();
#endif // !NDEBUG

    set_bytes(bytes_sent, sent_bytes);
    return return_value;
}

rtp_error_t uvgrtp::socket::sendto(sockaddr_in& addr, sockaddr_in6& addr6, pkt_vec& buffers, int send_flags)
{
    rtp_error_t ret = RTP_OK;
    for (auto& buffer : buffers) {
        std::lock_guard<std::mutex> lg(handlers_mutex_);
        for (auto& handler : vec_handlers_) {
            if ((ret = (*handler.second.handler)(handler.second.arg, buffer)) != RTP_OK) {
                UVG_LOG_ERROR("Malformed packet");
                return ret;
            }
        }
    }
    return __sendtov(addr, addr6, ipv6_, buffers, send_flags, nullptr);
}

rtp_error_t uvgrtp::socket::sendto(sockaddr_in& addr, sockaddr_in6& addr6, pkt_vec& buffers, int send_flags, int *bytes_sent)
{
    rtp_error_t ret = RTP_OK;

    for (auto& buffer : buffers) {
        std::lock_guard<std::mutex> lg(handlers_mutex_);
        for (auto& handler : vec_handlers_) {
            if ((ret = (*handler.second.handler)(handler.second.arg, buffer)) != RTP_OK) {
                UVG_LOG_ERROR("Malformed packet");
                return ret;
            }
        }
    }
    return __sendtov(addr, addr6, ipv6_, buffers, send_flags, bytes_sent);
}

rtp_error_t uvgrtp::socket::__recv(uint8_t *buf, size_t buf_len, int recv_flags, int *bytes_read)
{
    if (!buf || !buf_len) {
        set_bytes(bytes_read, -1);
        return RTP_INVALID_VALUE;
    }

#ifndef _WIN32
    int32_t ret = ::recv(socket_, buf, buf_len, recv_flags);

    if (ret == -1) {
        if (errno == EAGAIN || errno == EINTR) {
            set_bytes(bytes_read, 0);
            return RTP_INTERRUPTED;
        }
        UVG_LOG_ERROR("recv(2) failed: %s", strerror(errno));

        set_bytes(bytes_read, -1);
        return RTP_GENERIC_ERROR;
    }

    set_bytes(bytes_read, ret);
#else
    (void)recv_flags;

    WSABUF DataBuf;
    DataBuf.len = (u_long)buf_len;
    DataBuf.buf = (char *)buf;
    DWORD bytes_received = 0; 
    DWORD d_recv_flags = 0;

    int rc = ::WSARecv(socket_, &DataBuf, 1, &bytes_received, &d_recv_flags, NULL, NULL);
    if (rc == SOCKET_ERROR) {
        int err = WSAGetLastError();
        if (err == WSA_IO_PENDING || err == WSAEWOULDBLOCK) {
            set_bytes(bytes_read, 0);
            return RTP_INTERRUPTED;
        }

        log_platform_error("WSARecv() failed");
        set_bytes(bytes_read, -1);
        return RTP_GENERIC_ERROR;
    }

    set_bytes(bytes_read, bytes_received);
#endif

#ifndef NDEBUG
    ++received_packets_;
#endif // !NDEBUG

    return RTP_OK;
}

rtp_error_t uvgrtp::socket::recv(uint8_t *buf, size_t buf_len, int recv_flags)
{
    return uvgrtp::socket::__recv(buf, buf_len, recv_flags, nullptr);
}

rtp_error_t uvgrtp::socket::recv(uint8_t *buf, size_t buf_len, int recv_flags, int *bytes_read)
{
    return uvgrtp::socket::__recv(buf, buf_len, recv_flags, bytes_read);
}

rtp_error_t uvgrtp::socket::__recvfrom(uint8_t *buf, size_t buf_len, int recv_flags, sockaddr_in *sender, int *bytes_read)
{
    socklen_t *len_ptr = nullptr;
    socklen_t len      = sizeof(sockaddr_in);

    if (sender)
        len_ptr = &len;

#ifndef _WIN32
    int32_t ret = ::recvfrom(socket_, buf, buf_len, recv_flags, (struct sockaddr *)sender, len_ptr);

    if (ret == -1) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            set_bytes(bytes_read, 0);
            return RTP_INTERRUPTED;
        }
        UVG_LOG_ERROR("recvfrom failed: %s", strerror(errno));

        set_bytes(bytes_read, -1);
        return RTP_GENERIC_ERROR;
    }

    set_bytes(bytes_read, ret);
#else

    (void)recv_flags;

    WSABUF DataBuf;
    DataBuf.len = (u_long)buf_len;
    DataBuf.buf = (char *)buf;
    DWORD bytes_received = 0;
    DWORD d_recv_flags = 0;

    int rc = ::WSARecvFrom(socket_, &DataBuf, 1, &bytes_received, &d_recv_flags, (SOCKADDR *)sender, (int *)len_ptr, NULL, NULL);

    if (WSAGetLastError() == WSAEWOULDBLOCK)
        return RTP_INTERRUPTED;

    int err = 0;
    if ((rc == SOCKET_ERROR) && (WSA_IO_PENDING != (err = WSAGetLastError()))) {
        /* win_get_last_error(); */
        set_bytes(bytes_read, -1);
        return RTP_GENERIC_ERROR;
    }

    set_bytes(bytes_read, bytes_received);
#endif

#ifndef NDEBUG
    ++received_packets_;
#endif // !NDEBUG

    return RTP_OK;
}

rtp_error_t uvgrtp::socket::__recvfrom_ip6(uint8_t* buf, size_t buf_len, int recv_flags, sockaddr_in6* sender, int* bytes_read)
{
    socklen_t* len_ptr = nullptr;
    socklen_t len = sizeof(sockaddr_in6);

    if (sender)
        len_ptr = &len;

#ifndef _WIN32
    int32_t ret = ::recvfrom(socket_, buf, buf_len, recv_flags, (struct sockaddr*)sender, len_ptr);

    if (ret == -1) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            set_bytes(bytes_read, 0);
            return RTP_INTERRUPTED;
        }
        UVG_LOG_ERROR("recvfrom failed: %s", strerror(errno));

        set_bytes(bytes_read, -1);
        return RTP_GENERIC_ERROR;
    }

    set_bytes(bytes_read, ret);
#else

    (void)recv_flags;

    WSABUF DataBuf;
    DataBuf.len = (u_long)buf_len;
    DataBuf.buf = (char*)buf;
    DWORD bytes_received = 0;
    DWORD d_recv_flags = 0;

    int rc = ::WSARecvFrom(socket_, &DataBuf, 1, &bytes_received, &d_recv_flags, (SOCKADDR*)sender, (int*)len_ptr, NULL, NULL);

    if (WSAGetLastError() == WSAEWOULDBLOCK)
        return RTP_INTERRUPTED;

    int err = 0;
    if ((rc == SOCKET_ERROR) && (WSA_IO_PENDING != (err = WSAGetLastError()))) {
        /* win_get_last_error(); */
        set_bytes(bytes_read, -1);
        return RTP_GENERIC_ERROR;
    }

    set_bytes(bytes_read, bytes_received);
#endif

#ifndef NDEBUG
    ++received_packets_;
#endif // !NDEBUG

    return RTP_OK;
}

rtp_error_t uvgrtp::socket::recvfrom(uint8_t *buf, size_t buf_len, int recv_flags, sockaddr_in *sender,
    sockaddr_in6 *sender6, int *bytes_read)
{
    if (ipv6_) {
        return __recvfrom_ip6(buf, buf_len, recv_flags, sender6, bytes_read);
    }
    return __recvfrom(buf, buf_len, recv_flags, sender, bytes_read);
}

rtp_error_t uvgrtp::socket::recvfrom(uint8_t *buf, size_t buf_len, int recv_flags, int *bytes_read)
{
    if (ipv6_) {
        return __recvfrom_ip6(buf, buf_len, recv_flags, nullptr, bytes_read);
    }
    return __recvfrom(buf, buf_len, recv_flags, nullptr, bytes_read);
}

rtp_error_t uvgrtp::socket::recvfrom(uint8_t *buf, size_t buf_len, int recv_flags, sockaddr_in *sender)
{
    return __recvfrom(buf, buf_len, recv_flags, sender, nullptr);
}

rtp_error_t uvgrtp::socket::recvfrom(uint8_t *buf, size_t buf_len, int recv_flags)
{
    return __recvfrom(buf, buf_len, recv_flags, nullptr, nullptr);
}
