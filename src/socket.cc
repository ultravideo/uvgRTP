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
using namespace uvg_rtp;
using namespace mingw;
#endif

#include <cstring>
#include <cassert>

#include "debug.hh"
#include "socket.hh"
#include "util.hh"

uvg_rtp::socket::socket(int flags):
    socket_(-1),
    flags_(flags)
{
}

uvg_rtp::socket::~socket()
{
#ifdef __linux__
    close(socket_);
#else
    closesocket(socket_);
#endif
}

rtp_error_t uvg_rtp::socket::init(short family, int type, int protocol)
{
    assert(family == AF_INET);

#ifdef _WIN32
    if ((socket_ = ::socket(family, type, protocol)) == INVALID_SOCKET) {
        win_get_last_error();
#else
    if ((socket_ = ::socket(family, type, protocol)) < 0) {
        LOG_ERROR("Failed to create socket: %s", strerror(errno));
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

rtp_error_t uvg_rtp::socket::setsockopt(int level, int optname, const void *optval, socklen_t optlen)
{
    if (::setsockopt(socket_, level, optname, (const char *)optval, optlen) < 0) {
        LOG_ERROR("Failed to set socket options: %s", strerror(errno));
        return RTP_GENERIC_ERROR;
    }

    return RTP_OK;
}

rtp_error_t uvg_rtp::socket::bind(short family, unsigned host, short port)
{
    assert(family == AF_INET);

    sockaddr_in addr = create_sockaddr(family, host, port);

    if (::bind(socket_, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
#ifdef _WIN32
        win_get_last_error();
#else
        fprintf(stderr, "%s\n", strerror(errno));
#endif
        LOG_ERROR("Biding to port %u failed!", port);
        return RTP_BIND_ERROR;
    }

    return RTP_OK;
}

sockaddr_in uvg_rtp::socket::create_sockaddr(short family, unsigned host, short port)
{
    assert(family == AF_INET);

    sockaddr_in addr;

    memset(&addr, 0, sizeof(addr));

    addr.sin_family      = family;
    addr.sin_port        = htons(port);
    addr.sin_addr.s_addr = htonl(host);

    return addr;
}

sockaddr_in uvg_rtp::socket::create_sockaddr(short family, std::string host, short port)
{
    assert(family == AF_INET);

    sockaddr_in addr;

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = family;

    inet_pton(AF_INET, host.c_str(), &addr.sin_addr);
    addr.sin_port = htons((uint16_t)port);

    return addr;
}

void uvg_rtp::socket::set_sockaddr(sockaddr_in addr)
{
    addr_ = addr;
}

socket_t& uvg_rtp::socket::get_raw_socket()
{
    return socket_;
}


rtp_error_t uvg_rtp::socket::install_handler(void *arg, packet_handler_buf handler)
{
    if (!handler)
        return RTP_INVALID_VALUE;

    socket_packet_handler hndlr;

    hndlr.arg = arg;
    hndlr.handlers.buf = handler;
    buf_handlers_.push_back(hndlr);

    return RTP_OK;
}

rtp_error_t uvg_rtp::socket::install_handler(void *arg, packet_handler_vec handler)
{
    if (!handler)
        return RTP_INVALID_VALUE;


    socket_packet_handler hndlr;

    hndlr.arg = arg;
    hndlr.handlers.vec = handler;
    vec_handlers_.push_back(hndlr);

    return RTP_OK;
}

rtp_error_t uvg_rtp::socket::__sendto(sockaddr_in& addr, uint8_t *buf, size_t buf_len, int flags, int *bytes_sent)
{
    int nsend = 0;

#ifdef __linux__
    if ((nsend = ::sendto(socket_, buf, buf_len, flags, (const struct sockaddr *)&addr, sizeof(addr_))) == -1) {
        LOG_ERROR("Failed to send data: %s", strerror(errno));

        if (bytes_sent)
            *bytes_sent = -1;
        return RTP_SEND_ERROR;
    }
#else
    DWORD sent_bytes;
    WSABUF data_buf;

    data_buf.buf = (char *)buf;
    data_buf.len = buf_len;

    if (WSASendTo(socket_, &data_buf, 1, &sent_bytes, flags, (const struct sockaddr *)&addr, sizeof(addr_), nullptr, nullptr) == -1) {
        win_get_last_error();

        if (bytes_sent)
            *bytes_sent = -1;
        return RTP_SEND_ERROR;
    }
    nsend = sent_bytes;
#endif

    if (bytes_sent)
        *bytes_sent = nsend;

    return RTP_OK;
}

rtp_error_t uvg_rtp::socket::sendto(uint8_t *buf, size_t buf_len, int flags)
{
    rtp_error_t ret;

    for (auto& handler : buf_handlers_) {
        if ((ret = (*handler.handlers.buf)(handler.arg, buf_len, buf)) != RTP_OK) {
            LOG_ERROR("Malfored packet %p %zu %d", buf, buf_len, flags);
            return ret;
        }
    }

    return __sendto(addr_, buf, buf_len, flags, nullptr);
}

rtp_error_t uvg_rtp::socket::sendto(uint8_t *buf, size_t buf_len, int flags, int *bytes_sent)
{
    rtp_error_t ret;

    for (auto& handler : buf_handlers_) {
        if ((ret = (*handler.handlers.buf)(handler.arg, buf_len, buf)) != RTP_OK) {
            LOG_ERROR("Malfored packet %p %zu %d", buf, buf_len, flags);
            return ret;
        }
    }

    return __sendto(addr_, buf, buf_len, flags, bytes_sent);
}

rtp_error_t uvg_rtp::socket::sendto(sockaddr_in& addr, uint8_t *buf, size_t buf_len, int flags, int *bytes_sent)
{
    rtp_error_t ret;

    for (auto& handler : buf_handlers_) {
        if ((ret = (*handler.handlers.buf)(handler.arg, buf_len, buf)) != RTP_OK) {
            LOG_ERROR("Malfored packet %p %zu %d", buf, buf_len, flags);
            return ret;
        }
    }

    return __sendto(addr, buf, buf_len, flags, bytes_sent);
}

rtp_error_t uvg_rtp::socket::sendto(sockaddr_in& addr, uint8_t *buf, size_t buf_len, int flags)
{
    rtp_error_t ret;

    for (auto& handler : buf_handlers_) {
        if ((ret = (*handler.handlers.buf)(handler.arg, buf_len, buf)) != RTP_OK) {
            LOG_ERROR("Malfored packet %p %zu %d", buf, buf_len, flags);
            return ret;
        }
    }

    return __sendto(addr, buf, buf_len, flags, nullptr);
}

rtp_error_t uvg_rtp::socket::__sendtov(
    sockaddr_in& addr,
    std::vector<std::pair<size_t, uint8_t *>> buffers,
    int flags, int *bytes_sent
)
{
#ifdef __linux__
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

    if (sendmmsg(socket_, &header_, 1, flags) < 0) {
        LOG_ERROR("Failed to send RTP frame: %s!", strerror(errno));
        set_bytes(bytes_sent, -1);
        return RTP_SEND_ERROR;
    }

    set_bytes(bytes_sent, sent_bytes);
    return RTP_OK;

#else
    DWORD sent_bytes;

    /* create WSABUFs from input buffers and send them at once */
    for (size_t i = 0; i < buffers.size(); ++i) {
        buffers_[i].len = buffers.at(i).first;
        buffers_[i].buf = (char *)buffers.at(i).second;
    }

    if (WSASendTo(socket_, buffers_, buffers.size(), &sent_bytes, flags, (SOCKADDR *)&addr, sizeof(addr_), nullptr, nullptr) == -1) {
        win_get_last_error();

        set_bytes(bytes_sent, -1);
        return RTP_SEND_ERROR;
    }

    set_bytes(bytes_sent, sent_bytes);
    return RTP_OK;
#endif
}

rtp_error_t uvg_rtp::socket::sendto(std::vector<std::pair<size_t, uint8_t *>> buffers, int flags)
{
    rtp_error_t ret;

    for (auto& handler : buf_handlers_) {
        if ((ret = (*handler.handlers.vec)(handler.arg, buffers)) != RTP_OK) {
            LOG_ERROR("Malfored packet");
            return ret;
        }
    }

    return __sendtov(addr_, buffers, flags, nullptr);
}

rtp_error_t uvg_rtp::socket::sendto(std::vector<std::pair<size_t, uint8_t *>> buffers, int flags, int *bytes_sent)
{
    rtp_error_t ret;

    for (auto& handler : buf_handlers_) {
        if ((ret = (*handler.handlers.vec)(handler.arg, buffers)) != RTP_OK) {
            LOG_ERROR("Malfored packet");
            return ret;
        }
    }

    return __sendtov(addr_, buffers, flags, bytes_sent);
}

rtp_error_t uvg_rtp::socket::sendto(sockaddr_in& addr, std::vector<std::pair<size_t, uint8_t *>> buffers, int flags)
{
    rtp_error_t ret;

    for (auto& handler : buf_handlers_) {
        if ((ret = (*handler.handlers.vec)(handler.arg, buffers)) != RTP_OK) {
            LOG_ERROR("Malfored packet");
            return ret;
        }
    }

    return __sendtov(addr, buffers, flags, nullptr);
}

rtp_error_t uvg_rtp::socket::sendto(
    sockaddr_in& addr,
    std::vector<std::pair<size_t, uint8_t *>> buffers,
    int flags, int *bytes_sent
)
{
    rtp_error_t ret;

    for (auto& handler : buf_handlers_) {
        if ((ret = (*handler.handlers.vec)(handler.arg, buffers)) != RTP_OK) {
            LOG_ERROR("Malfored packet");
            return ret;
        }
    }

    return __sendtov(addr, buffers, flags, bytes_sent);
}

rtp_error_t uvg_rtp::socket::send_vecio(vecio_buf *buffers, size_t nbuffers, int flags)
{
    if (buffers == nullptr || nbuffers == 0)
        return RTP_INVALID_VALUE;

#ifdef __linux__
    size_t npkts = (flags_ & RCE_NO_SYSTEM_CALL_CLUSTERING) ? 1 : 1024;

    while (nbuffers > npkts) {
        if (sendmmsg(socket_, buffers, npkts, flags) < 0) {
            LOG_ERROR("Failed to flush the message queue: %s", strerror(errno));
            return RTP_SEND_ERROR;
        }

        nbuffers -= npkts;
        buffers  += npkts;
    }

    if (sendmmsg(socket_, buffers, nbuffers, flags) < 0) {
        LOG_ERROR("Failed to flush the message queue: %s", strerror(errno));
        return RTP_SEND_ERROR;
    }

    return RTP_OK;
#else
    /* TODO:  */
#endif
}

rtp_error_t uvg_rtp::socket::recv_vecio(vecio_buf *buffers, size_t nbuffers, int flags, int *nread)
{
    if (buffers == nullptr || nbuffers == 0)
        return RTP_INVALID_VALUE;

    ssize_t dgrams_read = 0;

#ifdef __linux__
    if (flags_ & RCE_NO_SYSTEM_CALL_CLUSTERING) {
        for (size_t i = 0; i < nbuffers; ++i) {
            if ((dgrams_read += ::recvmmsg(socket_, &buffers[i], 1, flags, nullptr)) < 0) {
                LOG_ERROR("recvmmsg(2) failed: %s", strerror(errno));
                set_bytes(nread, -1);
                return RTP_GENERIC_ERROR;
            }
        }
    } else {
        if ((dgrams_read = ::recvmmsg(socket_, buffers, nbuffers, flags, nullptr)) < 0) {
            LOG_ERROR("recvmmsg(2) failed: %s", strerror(errno));
            set_bytes(nread, -1);
            return RTP_GENERIC_ERROR;
        }
    }

    set_bytes(nread, dgrams_read);
    return RTP_OK;
#else
    /* TODO:  */
#endif
}

rtp_error_t uvg_rtp::socket::__recv(uint8_t *buf, size_t buf_len, int flags, int *bytes_read)
{
    if (!buf || !buf_len) {
        set_bytes(bytes_read, -1);
        return RTP_INVALID_VALUE;
    }

#ifdef __linux__
    int32_t ret = ::recv(socket_, buf, buf_len, flags);

    if (ret == -1) {
        if (errno == EAGAIN) {
            set_bytes(bytes_read, 0);
            return RTP_INTERRUPTED;
        }
        LOG_ERROR("recv(2) failed: %s", strerror(errno));

        set_bytes(bytes_read, -1);
        return RTP_GENERIC_ERROR;
    }

    set_bytes(bytes_read, ret);
    return RTP_OK;
#else
    int rc, err;
    WSABUF DataBuf;
    DataBuf.len = (u_long)buf_len;
    DataBuf.buf = (char *)buf;
    DWORD bytes_received, flags_ = 0;

    rc = ::WSARecv(socket_, &DataBuf, 1, &bytes_received, &flags_, NULL, NULL);

    if ((rc == SOCKET_ERROR) && (WSA_IO_PENDING != (err = WSAGetLastError()))) {
        win_get_last_error();
        set_bytes(bytes_read, -1);
        return RTP_GENERIC_ERROR;
    }

    set_bytes(bytes_read, bytes_received);
    return RTP_OK;
#endif
}

rtp_error_t uvg_rtp::socket::recv(uint8_t *buf, size_t buf_len, int flags)
{
    return uvg_rtp::socket::__recv(buf, buf_len, flags, nullptr);
}

rtp_error_t uvg_rtp::socket::recv(uint8_t *buf, size_t buf_len, int flags, int *bytes_read)
{
    return uvg_rtp::socket::__recv(buf, buf_len, flags, bytes_read);
}

rtp_error_t uvg_rtp::socket::__recvfrom(uint8_t *buf, size_t buf_len, int flags, sockaddr_in *sender, int *bytes_read)
{
    socklen_t *len_ptr = nullptr;
    socklen_t len      = sizeof(sockaddr_in);

    if (sender)
        len_ptr = &len;

#ifdef __linux__
    int32_t ret = ::recvfrom(socket_, buf, buf_len, flags, (struct sockaddr *)sender, len_ptr);

    if (ret == -1) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            set_bytes(bytes_read, 0);
            return RTP_INTERRUPTED;
        }
        LOG_ERROR("recvfrom failed: %s", strerror(errno));

        set_bytes(bytes_read, -1);
        return RTP_GENERIC_ERROR;
    }

    set_bytes(bytes_read, ret);
    return RTP_OK;
#else
    int rc, err;
    WSABUF DataBuf;
    DataBuf.len = (u_long)buf_len;
    DataBuf.buf = (char *)buf;
    DWORD bytes_received, flags_ = 0;

    rc = ::WSARecvFrom(socket_, &DataBuf, 1, &bytes_received, &flags_, (SOCKADDR *)sender, (int *)len_ptr, NULL, NULL);

    if (WSAGetLastError() == WSAEWOULDBLOCK)
        return RTP_INTERRUPTED;

    if ((rc == SOCKET_ERROR) && (WSA_IO_PENDING != (err = WSAGetLastError()))) {
        /* win_get_last_error(); */
        set_bytes(bytes_read, -1);
        return RTP_GENERIC_ERROR;
    }

    set_bytes(bytes_read, bytes_received);
    return RTP_OK;
#endif
}

rtp_error_t uvg_rtp::socket::recvfrom(uint8_t *buf, size_t buf_len, int flags, sockaddr_in *sender, int *bytes_read)
{
    return __recvfrom(buf, buf_len, flags, sender, bytes_read);
}

rtp_error_t uvg_rtp::socket::recvfrom(uint8_t *buf, size_t buf_len, int flags, int *bytes_read)
{
    return __recvfrom(buf, buf_len, flags, nullptr, bytes_read);
}

rtp_error_t uvg_rtp::socket::recvfrom(uint8_t *buf, size_t buf_len, int flags, sockaddr_in *sender)
{
    return __recvfrom(buf, buf_len, flags, sender, nullptr);
}

rtp_error_t uvg_rtp::socket::recvfrom(uint8_t *buf, size_t buf_len, int flags)
{
    return __recvfrom(buf, buf_len, flags, nullptr, nullptr);
}

sockaddr_in& uvg_rtp::socket::get_out_address()
{
    return addr_;
}
