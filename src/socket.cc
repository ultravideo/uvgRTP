#ifdef _WIN32
#include <winsock2.h>
#include <ws2def.h>
#else
#include <unistd.h>
#include <poll.h>
#include <fcntl.h>
#endif

#if defined(__MINGW32__) || defined(__MINGW64__)
#include "mingw_inet.hh"
using namespace kvz_rtp;
using namespace mingw;
#endif

#include <cstring>
#include <cassert>

#include "debug.hh"
#include "socket.hh"

kvz_rtp::socket::socket():
    socket_(-1)
{
}

kvz_rtp::socket::~socket()
{
#ifdef __linux__
    close(socket_);
#else
    /* TODO: winsock stuff? */
#endif
}

rtp_error_t kvz_rtp::socket::init(short family, int type, int protocol)
{
    assert(family == AF_INET);

#ifdef _WIN32
    if ((socket_ = ::socket(family, type, protocol)) == INVALID_SOCKET) {
        LOG_ERROR("todo windows specific error message");
#else
    if ((socket_ = ::socket(family, type, protocol)) < 0) {
        LOG_ERROR("Failed to create socket: %s", strerror(errno));
#endif
        return RTP_SOCKET_ERROR;
    }

    return RTP_OK;
}

rtp_error_t kvz_rtp::socket::setsockopt(int level, int optname, const void *optval, socklen_t optlen)
{
    if (::setsockopt(socket_, level, optname, (const char *)optval, optlen) < 0) {
        LOG_ERROR("Failed to set socket options: %s", strerror(errno));
        return RTP_GENERIC_ERROR;
    }

    return RTP_OK;
}

rtp_error_t kvz_rtp::socket::bind(short family, unsigned host, short port)
{
    assert(family == AF_INET);

    sockaddr_in addr = create_sockaddr(family, host, port);

    if (::bind(socket_, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        LOG_ERROR("Biding to port %u failed!", port);
        return RTP_BIND_ERROR;
    }

    return RTP_OK;
}

sockaddr_in kvz_rtp::socket::create_sockaddr(short family, unsigned host, short port)
{
    assert(family == AF_INET);

    sockaddr_in addr;

    memset(&addr, 0, sizeof(addr));

    addr.sin_family      = family;
    addr.sin_port        = htons(port);
    addr.sin_addr.s_addr = htonl(host);

    return addr;
}

sockaddr_in kvz_rtp::socket::create_sockaddr(short family, std::string host, short port)
{
    assert(family == AF_INET);

    sockaddr_in addr;

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = family;

    inet_pton(AF_INET, host.c_str(), &addr.sin_addr);
    addr.sin_port = htons((uint16_t)port);

    return addr;
}

void kvz_rtp::socket::set_sockaddr(sockaddr_in addr)
{
    addr_ = addr;
}

kvz_rtp::socket_t& kvz_rtp::socket::get_raw_socket()
{
    return socket_;
}

rtp_error_t kvz_rtp::socket::__sendto(sockaddr_in& addr, uint8_t *buf, size_t buf_len, int flags, int *bytes_sent)
{
    int nsend;

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

    if (WSASend(socket_, &data_buf, 1, &sent_bytes, flags, NULL, NULL) == -1) {
        /* TODO: winsock specific error message */
        LOG_ERROR("Failed to send data!");

        if (bytes_sent)
            *bytes_sent = -1;
        return RTP_SEND_ERROR;
    }
#endif

    if (bytes_sent)
        *bytes_sent = nsend;

    return RTP_OK;
}

rtp_error_t kvz_rtp::socket::sendto(uint8_t *buf, size_t buf_len, int flags, int *bytes_sent)
{
    return __sendto(addr_, buf, buf_len, flags, bytes_sent);
}

rtp_error_t kvz_rtp::socket::sendto(sockaddr_in& addr, uint8_t *buf, size_t buf_len, int flags, int *bytes_sent)
{
    return __sendto(addr, buf, buf_len, flags, bytes_sent);
}

rtp_error_t kvz_rtp::socket::sendto(sockaddr_in& addr, uint8_t *buf, size_t buf_len, int flags)
{
    return __sendto(addr, buf, buf_len, flags, NULL);
}

rtp_error_t kvz_rtp::socket::__recvfrom(uint8_t *buf, size_t buf_len, int flags, sockaddr_in *sender, int *bytes_read)
{
    socklen_t *len_ptr = NULL;
    socklen_t len      = sizeof(sockaddr_in);

    if (sender)
        len_ptr = &len;

#ifdef __linux__
    int32_t ret = ::recvfrom(socket_, buf, buf_len, flags, (struct sockaddr *)sender, len_ptr);

    if (ret == -1) {
        if (errno == EAGAIN) {
            if (bytes_read)
                bytes_read = 0;
            return RTP_INTERRUPTED;
        }

        LOG_ERROR("recvfrom failed: %s", strerror(errno));

        if (bytes_read)
            *bytes_read = -1;
        return RTP_GENERIC_ERROR;
    }
#else
    int32_t ret = ::recvfrom(socket_, (char *)buf, buf_len, flags, (SOCKADDR *)sender, (int *)len_ptr);

    if (ret == -1) {
        LOG_ERROR("recvfrom failed: %d", WSAGetLastError());

        if (bytes_read)
            *bytes_read = -1;
        return RTP_GENERIC_ERROR;
    }
#endif

    if (bytes_read)
        *bytes_read = ret;

    return RTP_OK;
}

rtp_error_t kvz_rtp::socket::recvfrom(uint8_t *buf, size_t buf_len, int flags, sockaddr_in *sender, int *bytes_read)
{
    return __recvfrom(buf, buf_len, flags, sender, bytes_read);
}

rtp_error_t kvz_rtp::socket::recvfrom(uint8_t *buf, size_t buf_len, int flags, int *bytes_read)
{
    return __recvfrom(buf, buf_len, flags, NULL, bytes_read);
}

rtp_error_t kvz_rtp::socket::recvfrom(uint8_t *buf, size_t buf_len, int flags, sockaddr_in *sender)
{
    return __recvfrom(buf, buf_len, flags, sender, NULL);
}

rtp_error_t kvz_rtp::socket::recvfrom(uint8_t *buf, size_t buf_len, int flags)
{
    return __recvfrom(buf, buf_len, flags, NULL, NULL);
}
