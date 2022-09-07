#include "socket.hh"

#include "uvgrtp/util.hh"

#include "debug.hh"

#include <thread>

#ifdef _WIN32
#include <winsock2.h>
#include <Ws2tcpip.h>
#include <ws2def.h>
#else
#include <unistd.h>
#include <poll.h>
#include <fcntl.h>
#endif

#if defined(__MINGW32__) || defined(__MINGW64__)
#include "mingw_inet.hh"
using namespace uvgrtp;
using namespace mingw;
#endif

#include <cstring>
#include <cassert>


#define WSABUF_SIZE 256

uvgrtp::socket::socket(int rce_flags):
    socket_(0),
    remote_address_(),
    local_address_(),
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
    assert(family == AF_INET);

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

rtp_error_t uvgrtp::socket::bind(sockaddr_in& local_address)
{
    local_address_ = local_address;

    UVG_LOG_DEBUG("Binding to address %s", sockaddr_to_string(local_address_).c_str());

    if (::bind(socket_, (struct sockaddr*)&local_address_, sizeof(local_address_)) < 0) {
#ifdef _WIN32
        win_get_last_error();
#else
        fprintf(stderr, "%s\n", strerror(errno));
#endif
        UVG_LOG_ERROR("Binding to port %u failed!", local_address_.sin_port);
        return RTP_BIND_ERROR;
    }

    return RTP_OK;
}

sockaddr_in uvgrtp::socket::create_sockaddr(short family, unsigned host, short port) const
{
    assert(family == AF_INET);

    sockaddr_in addr;

    memset(&addr, 0, sizeof(addr));

    addr.sin_family      = family;
    addr.sin_port        = htons(port);
    addr.sin_addr.s_addr = htonl(host);

    return addr;
}

sockaddr_in uvgrtp::socket::create_sockaddr(short family, std::string host, short port) const
{
    assert(family == AF_INET);

    sockaddr_in addr;

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = family;

    inet_pton(AF_INET, host.c_str(), &addr.sin_addr);
    addr.sin_port = htons((uint16_t)port);

    return addr;
}

std::string uvgrtp::socket::get_socket_path_string() const
{
    return sockaddr_to_string(local_address_) + " -> " + sockaddr_to_string(remote_address_);
}

std::string uvgrtp::socket::sockaddr_to_string(const sockaddr_in& addr) const
{
    char* c_string = new char[INET_ADDRSTRLEN];
    memset(c_string, 0, INET_ADDRSTRLEN);

    switch (addr.sin_family)
    {
    case AF_INET:
    {
        inet_ntop(AF_INET, &addr.sin_addr, c_string, INET_ADDRSTRLEN);
        break;
    }
    case AF_INET6:
    {
        inet_ntop(AF_INET6, &addr.sin_addr, c_string, INET_ADDRSTRLEN);
        break;
    }
    }

    std::string string(c_string);
    string.append(":" + std::to_string(ntohs(addr.sin_port)));

    delete[] c_string;
    return string;
}

void uvgrtp::socket::set_sockaddr(sockaddr_in addr)
{
    remote_address_ = addr;
}

socket_t& uvgrtp::socket::get_raw_socket()
{
    return socket_;
}

rtp_error_t uvgrtp::socket::install_handler(void *arg, packet_handler_vec handler)
{
    if (!handler)
        return RTP_INVALID_VALUE;


    socket_packet_handler hndlr;

    hndlr.arg = arg;
    hndlr.handler = handler;
    vec_handlers_.push_back(hndlr);

    return RTP_OK;
}

rtp_error_t uvgrtp::socket::__sendto(sockaddr_in& addr, uint8_t *buf, size_t buf_len, int send_flags, int *bytes_sent)
{
    int nsend = 0;

#ifndef _WIN32
    if ((nsend = ::sendto(socket_, buf, buf_len, send_flags, (const struct sockaddr *)&addr, sizeof(addr))) == -1) {
        UVG_LOG_ERROR("Failed to send data: %s", strerror(errno));

        if (bytes_sent)
            *bytes_sent = -1;
        return RTP_SEND_ERROR;
    }
#else
    DWORD sent_bytes = 0;
    WSABUF data_buf;

    data_buf.buf = (char *)buf;
    data_buf.len = (ULONG)buf_len;

    if (WSASendTo(socket_, &data_buf, 1, &sent_bytes, send_flags, (const struct sockaddr *)&addr, sizeof(addr), nullptr, nullptr) == -1) {
        win_get_last_error();

        UVG_LOG_ERROR("Failed to send to %s", sockaddr_to_string(addr).c_str());

        if (bytes_sent)
            *bytes_sent = -1;
        return RTP_SEND_ERROR;
    }
    nsend = sent_bytes;
#endif

    if (bytes_sent)
        *bytes_sent = nsend;

#ifndef NDEBUG
    ++sent_packets_;
#endif // !NDEBUG

    return RTP_OK;
}

rtp_error_t uvgrtp::socket::sendto(uint8_t *buf, size_t buf_len, int send_flags)
{
    return __sendto(remote_address_, buf, buf_len, send_flags, nullptr);
}

rtp_error_t uvgrtp::socket::sendto(uint8_t *buf, size_t buf_len, int send_flags, int *bytes_sent)
{
    return __sendto(remote_address_, buf, buf_len, send_flags, bytes_sent);
}

rtp_error_t uvgrtp::socket::sendto(sockaddr_in& addr, uint8_t *buf, size_t buf_len, int send_flags, int *bytes_sent)
{
    return __sendto(addr, buf, buf_len, send_flags, bytes_sent);
}

rtp_error_t uvgrtp::socket::sendto(sockaddr_in& addr, uint8_t *buf, size_t buf_len, int send_flags)
{
    return __sendto(addr, buf, buf_len, send_flags, nullptr);
}

rtp_error_t uvgrtp::socket::__sendtov(
    sockaddr_in& addr,
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

    header_.msg_hdr.msg_name       = (void *)&addr;
    header_.msg_hdr.msg_namelen    = sizeof(addr);
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

    if (WSASendTo(socket_, buffers_, (DWORD)buffers.size(), &sent_bytes, send_flags,
                  (SOCKADDR *)&addr, sizeof(addr), nullptr, nullptr) == -1) {
        win_get_last_error();

        UVG_LOG_ERROR("Failed to send to %s", sockaddr_to_string(addr).c_str());

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

rtp_error_t uvgrtp::socket::sendto(buf_vec& buffers, int send_flags)
{
    rtp_error_t ret = RTP_OK;

    for (auto& handler : vec_handlers_) {
        if ((ret = (*handler.handler)(handler.arg, buffers)) != RTP_OK) {
            UVG_LOG_ERROR("Malformed packet");
            return ret;
        }
    }

    return __sendtov(remote_address_, buffers, send_flags, nullptr);
}

rtp_error_t uvgrtp::socket::sendto(buf_vec& buffers, int send_flags, int *bytes_sent)
{
    rtp_error_t ret = RTP_OK;

    for (auto& handler : vec_handlers_) {
        if ((ret = (*handler.handler)(handler.arg, buffers)) != RTP_OK) {
            UVG_LOG_ERROR("Malformed packet");
            return ret;
        }
    }

    return __sendtov(remote_address_, buffers, send_flags, bytes_sent);
}

rtp_error_t uvgrtp::socket::sendto(sockaddr_in& addr, buf_vec& buffers, int send_flags)
{
    rtp_error_t ret = RTP_OK;

    for (auto& handler : vec_handlers_) {
        if ((ret = (*handler.handler)(handler.arg, buffers)) != RTP_OK) {
            UVG_LOG_ERROR("Malformed packet");
            return ret;
        }
    }

    return __sendtov(addr, buffers, send_flags, nullptr);
}

rtp_error_t uvgrtp::socket::sendto(
    sockaddr_in& addr,
    buf_vec& buffers,
    int send_flags, int *bytes_sent
)
{
    rtp_error_t ret = RTP_OK;

    for (auto& handler : vec_handlers_) {
        if ((ret = (*handler.handler)(handler.arg, buffers)) != RTP_OK) {
            UVG_LOG_ERROR("Malformed packet");
            return ret;
        }
    }

    return __sendtov(addr, buffers, send_flags, bytes_sent);
}

rtp_error_t uvgrtp::socket::__sendtov(
    sockaddr_in& addr,
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
        headers[i].msg_hdr.msg_name       = (void *)&addr;
        headers[i].msg_hdr.msg_namelen    = sizeof(addr);
        headers[i].msg_hdr.msg_control    = 0;
        headers[i].msg_hdr.msg_controllen = 0;

        for (size_t k = 0; k < buffers[i].size(); ++k) {
            headers[i].msg_hdr.msg_iov[k].iov_len   = buffers[i][k].first;
            headers[i].msg_hdr.msg_iov[k].iov_base  = buffers[i][k].second;
            sent_bytes                             += buffers[i][k].first;
        }
    }

    ssize_t npkts = (rce_flags_ & RCE_NO_SYSTEM_CALL_CLUSTERING) ? 1 : 1024;
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
        ret = WSASendTo(
            socket_,
            wsa_bufs,
            (DWORD)buffer.size(),
            &sent_bytes_dw,
            send_flags,
            (SOCKADDR *)&addr,
            sizeof(addr),
            nullptr,
            nullptr
        );

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

                UVG_LOG_ERROR("Failed to send to %s", sockaddr_to_string(addr).c_str());
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

rtp_error_t uvgrtp::socket::sendto(pkt_vec& buffers, int send_flags)
{
    rtp_error_t ret = RTP_OK;

    for (auto& buffer : buffers) {
        for (auto& handler : vec_handlers_) {
            if ((ret = (*handler.handler)(handler.arg, buffer)) != RTP_OK) {
                UVG_LOG_ERROR("Malformed packet");
                return ret;
            }
        }
    }

    return __sendtov(remote_address_, buffers, send_flags, nullptr);
}

rtp_error_t uvgrtp::socket::sendto(pkt_vec& buffers, int send_flags, int *bytes_sent)
{
    rtp_error_t ret = RTP_OK;

    for (auto& buffer : buffers) {
        for (auto& handler : vec_handlers_) {
            if ((ret = (*handler.handler)(handler.arg, buffer)) != RTP_OK) {
                UVG_LOG_ERROR("Malformed packet");
                return ret;
            }
        }
    }

    return __sendtov(remote_address_, buffers, send_flags, bytes_sent);
}

rtp_error_t uvgrtp::socket::sendto(sockaddr_in& addr, pkt_vec& buffers, int send_flags)
{
    rtp_error_t ret = RTP_OK;

    for (auto& buffer : buffers) {
        for (auto& handler : vec_handlers_) {
            if ((ret = (*handler.handler)(handler.arg, buffer)) != RTP_OK) {
                UVG_LOG_ERROR("Malformed packet");
                return ret;
            }
        }
    }

    return __sendtov(addr, buffers, send_flags, nullptr);
}

rtp_error_t uvgrtp::socket::sendto(sockaddr_in& addr, pkt_vec& buffers, int send_flags, int *bytes_sent)
{
    rtp_error_t ret = RTP_OK;

    for (auto& buffer : buffers) {
        for (auto& handler : vec_handlers_) {
            if ((ret = (*handler.handler)(handler.arg, buffer)) != RTP_OK) {
                UVG_LOG_ERROR("Malformed packet");
                return ret;
            }
        }
    }

    return __sendtov(addr, buffers, send_flags, bytes_sent);
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

rtp_error_t uvgrtp::socket::recvfrom(uint8_t *buf, size_t buf_len, int recv_flags, sockaddr_in *sender, int *bytes_read)
{
    return __recvfrom(buf, buf_len, recv_flags, sender, bytes_read);
}

rtp_error_t uvgrtp::socket::recvfrom(uint8_t *buf, size_t buf_len, int recv_flags, int *bytes_read)
{
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

sockaddr_in& uvgrtp::socket::get_out_address()
{
    return remote_address_;
}
