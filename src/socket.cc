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
#include "util.hh"

kvz_rtp::socket::socket():
    recv_handler_(nullptr),
    sendto_handler_(nullptr),
    sendtov_handler_(nullptr),
    socket_(-1),
    srtp_(nullptr)
{
}

kvz_rtp::socket::~socket()
{
#ifdef __linux__
    close(socket_);
#else
    closesocket(socket_);
#endif

    delete srtp_;
}

rtp_error_t kvz_rtp::socket::setup_srtp(uint32_t ssrc)
{
    if (srtp_) {
        LOG_DEBUG("SRTP has already been initialized");
        return RTP_INITIALIZED;
    }

    if ((srtp_ = new kvz_rtp::srtp(SRTP)) == nullptr) {
        LOG_DEBUG("Failed to allocate SRTP context!");
        return RTP_MEMORY_ERROR;
    }

    return srtp_->init_zrtp(ssrc, socket_, addr_);
}

rtp_error_t kvz_rtp::socket::setup_srtcp(uint32_t ssrc)
{
    if (srtp_) {
        LOG_DEBUG("SRTP has already been initialized");
        return RTP_INITIALIZED;
    }

    if ((srtp_ = new kvz_rtp::srtp(SRTCP)) == nullptr) {
        LOG_DEBUG("Failed to allocate SRTP context!");
        return RTP_MEMORY_ERROR;
    }

    return srtp_->init_zrtp(ssrc, socket_, addr_);
}

rtp_error_t kvz_rtp::socket::setup_srtp(uint32_t ssrc, std::pair<uint8_t *, size_t>& key)
{
    if (srtp_) {
        LOG_DEBUG("SRTP has already been initialized");
        return RTP_INITIALIZED;
    }

    if ((srtp_ = new kvz_rtp::srtp(SRTP)) == nullptr) {
        LOG_DEBUG("Failed to allocate SRTP context!");
        return RTP_MEMORY_ERROR;
    }

    return srtp_->init_user(ssrc, key);
}

rtp_error_t kvz_rtp::socket::setup_srtcp(uint32_t ssrc, std::pair<uint8_t *, size_t>& key)
{
    if (srtp_) {
        LOG_DEBUG("SRTP has already been initialized");
        return RTP_INITIALIZED;
    }

    if ((srtp_ = new kvz_rtp::srtp(SRTCP)) == nullptr) {
        LOG_DEBUG("Failed to allocate SRTP context!");
        return RTP_MEMORY_ERROR;
    }

    return srtp_->init_user(ssrc, key);
}

rtp_error_t kvz_rtp::socket::init(short family, int type, int protocol)
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

socket_t& kvz_rtp::socket::get_raw_socket()
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

    if (WSASendTo(socket_, &data_buf, 1, &sent_bytes, flags, (const struct sockaddr *)&addr, sizeof(addr_), nullptr, nullptr) == -1) {
        win_get_last_error();

        if (bytes_sent)
            *bytes_sent = -1;
        return RTP_SEND_ERROR;
    }
#endif

    if (bytes_sent)
        *bytes_sent = nsend;

    return RTP_OK;
}

rtp_error_t kvz_rtp::socket::sendto(uint8_t *buf, size_t buf_len, int flags)
{
    if (sendto_handler_)
        return sendto_handler_(socket_, addr_, buf, buf_len, flags, nullptr);

    return __sendto(addr_, buf, buf_len, flags, nullptr);
}

rtp_error_t kvz_rtp::socket::sendto(uint8_t *buf, size_t buf_len, int flags, int *bytes_sent)
{
    if (sendto_handler_)
        return sendto_handler_(socket_, addr_, buf, buf_len, flags, bytes_sent);

    return __sendto(addr_, buf, buf_len, flags, bytes_sent);
}

rtp_error_t kvz_rtp::socket::sendto(sockaddr_in& addr, uint8_t *buf, size_t buf_len, int flags, int *bytes_sent)
{
    if (sendto_handler_)
        return sendto_handler_(socket_, addr, buf, buf_len, flags, bytes_sent);

    return __sendto(addr, buf, buf_len, flags, bytes_sent);
}

rtp_error_t kvz_rtp::socket::sendto(sockaddr_in& addr, uint8_t *buf, size_t buf_len, int flags)
{
    if (sendto_handler_)
        return sendto_handler_(socket_, addr, buf, buf_len, flags, nullptr);

    return __sendto(addr, buf, buf_len, flags, nullptr);
}

rtp_error_t kvz_rtp::socket::__sendtov(
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

rtp_error_t kvz_rtp::socket::sendto(std::vector<std::pair<size_t, uint8_t *>> buffers, int flags)
{
    if (sendtov_handler_)
        return sendtov_handler_(socket_, addr_, buffers, flags, nullptr);

    return __sendtov(addr_, buffers, flags, nullptr);
}

rtp_error_t kvz_rtp::socket::sendto(std::vector<std::pair<size_t, uint8_t *>> buffers, int flags, int *bytes_sent)
{
    if (sendtov_handler_)
        return sendtov_handler_(socket_, addr_, buffers, flags, bytes_sent);

    return __sendtov(addr_, buffers, flags, bytes_sent);
}

rtp_error_t kvz_rtp::socket::sendto(sockaddr_in& addr, std::vector<std::pair<size_t, uint8_t *>> buffers, int flags)
{
    if (sendtov_handler_)
        return sendtov_handler_(socket_, addr, buffers, flags, nullptr);

    return __sendtov(addr, buffers, flags, nullptr);
}

rtp_error_t kvz_rtp::socket::sendto(
    sockaddr_in& addr,
    std::vector<std::pair<size_t, uint8_t *>> buffers,
    int flags, int *bytes_sent
)
{
    if (sendtov_handler_)
        return sendtov_handler_(socket_, addr, buffers, flags, bytes_sent);

    return __sendtov(addr, buffers, flags, bytes_sent);
}

rtp_error_t kvz_rtp::socket::send_vecio(vecio_buf *buffers, size_t nbuffers, int flags)
{
    if (buffers == nullptr || nbuffers == 0)
        return RTP_INVALID_VALUE;

#ifdef __linux__
    while (nbuffers > 1024) {
        if (sendmmsg(socket_, buffers, 1024, flags) < 0) {
            LOG_ERROR("Failed to flush the message queue: %s", strerror(errno));
            return RTP_SEND_ERROR;
        }

        nbuffers -= 1024;
        buffers  += 1024;
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

rtp_error_t kvz_rtp::socket::recv_vecio(vecio_buf *buffers, size_t nbuffers, int flags, int *nread)
{
    if (buffers == nullptr || nbuffers == 0)
        return RTP_INVALID_VALUE;

    ssize_t dgrams_read = 0;

#ifdef __linux__
    if ((dgrams_read = ::recvmmsg(socket_, buffers, nbuffers, flags, nullptr)) < 0) {
        LOG_ERROR("recvmmsg(2) failed: %s", strerror(errno));
        set_bytes(nread, -1);
        return RTP_GENERIC_ERROR;
    }

    set_bytes(nread, dgrams_read);
    return RTP_OK;
#else
    /* TODO:  */
#endif
}

rtp_error_t kvz_rtp::socket::__recv(uint8_t *buf, size_t buf_len, int flags, int *bytes_read)
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

rtp_error_t kvz_rtp::socket::recv(uint8_t *buf, size_t buf_len, int flags)
{
    return kvz_rtp::socket::__recv(buf, buf_len, flags, nullptr);
}

rtp_error_t kvz_rtp::socket::recv(uint8_t *buf, size_t buf_len, int flags, int *bytes_read)
{
    return kvz_rtp::socket::__recv(buf, buf_len, flags, bytes_read);
}

rtp_error_t kvz_rtp::socket::__recvfrom(uint8_t *buf, size_t buf_len, int flags, sockaddr_in *sender, int *bytes_read)
{
    socklen_t *len_ptr = nullptr;
    socklen_t len      = sizeof(sockaddr_in);

    if (sender)
        len_ptr = &len;

#ifdef __linux__
    int32_t ret = ::recvfrom(socket_, buf, buf_len, flags, (struct sockaddr *)sender, len_ptr);

    if (ret == -1) {
        if (errno == EAGAIN) {
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

    if ((rc == SOCKET_ERROR) && (WSA_IO_PENDING != (err = WSAGetLastError()))) {
        win_get_last_error();
        set_bytes(bytes_read, -1);
        return RTP_GENERIC_ERROR;
    }

    set_bytes(bytes_read, bytes_received);
    return RTP_OK;
#endif
}

rtp_error_t kvz_rtp::socket::recvfrom(uint8_t *buf, size_t buf_len, int flags, sockaddr_in *sender, int *bytes_read)
{
    if (recv_handler_)
        return recv_handler_(socket_, buf, buf_len, flags, sender, bytes_read);

    return __recvfrom(buf, buf_len, flags, sender, bytes_read);
}

rtp_error_t kvz_rtp::socket::recvfrom(uint8_t *buf, size_t buf_len, int flags, int *bytes_read)
{
    if (recv_handler_)
        return recv_handler_(socket_, buf, buf_len, flags, nullptr, bytes_read);

    return __recvfrom(buf, buf_len, flags, nullptr, bytes_read);
}

rtp_error_t kvz_rtp::socket::recvfrom(uint8_t *buf, size_t buf_len, int flags, sockaddr_in *sender)
{
    if (recv_handler_)
        return recv_handler_(socket_, buf, buf_len, flags, sender, nullptr);

    return __recvfrom(buf, buf_len, flags, sender, nullptr);
}

rtp_error_t kvz_rtp::socket::recvfrom(uint8_t *buf, size_t buf_len, int flags)
{
    if (recv_handler_)
        return recv_handler_(socket_, buf, buf_len, flags, nullptr, nullptr);

    return __recvfrom(buf, buf_len, flags, nullptr, nullptr);
}

void kvz_rtp::socket::install_ll_recv(
    rtp_error_t (*recv)(socket_t, uint8_t *, size_t , int , sockaddr_in *, int *)
)
{
    assert(recv != nullptr);
    recv_handler_ = recv;
}

void kvz_rtp::socket::install_ll_sendto(
    rtp_error_t (*sendto)(socket_t, sockaddr_in&, uint8_t *, size_t, int, int *)
)
{
    assert(sendto != nullptr);
    sendto_handler_ = sendto;
}

void kvz_rtp::socket::install_ll_sendtov(
    rtp_error_t (*sendtov)(socket_t, sockaddr_in&, std::vector<std::pair<size_t, uint8_t *>>, int, int *)
)
{
    assert(sendtov != nullptr);
    sendtov_handler_ = sendtov;
}

sockaddr_in& kvz_rtp::socket::get_out_address()
{
    return addr_;
}
