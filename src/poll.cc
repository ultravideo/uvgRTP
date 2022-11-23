#include "poll.hh"

#include "socket.hh"
#include "debug.hh"
#include "memory.hh"

#ifdef _WIN32
#include <winsock2.h>
#else
#include <unistd.h>
#include <poll.h>
#endif

#include <cstring>

const int MULTICAST_MAX_PEERS = 64;


rtp_error_t uvgrtp::poll::blocked_recv(std::shared_ptr<uvgrtp::socket> socket, uint8_t *buf, size_t buf_len, int timeout, int *bytes_read)
{
    if (!buf|| !buf_len)
        return RTP_INVALID_VALUE;

    fd_set read_fds;
    rtp_error_t rtp_ret;

    FD_ZERO(&read_fds);
    FD_SET(socket->get_raw_socket(), &read_fds);

    size_t msec = timeout % 1000;
    size_t sec  = timeout - msec;

    struct timeval t_val = {
        (int)sec  / 1000,
        (int)msec * 1000,
    };

    int ret = ::select(1, &read_fds, nullptr, nullptr, &t_val);

    if (ret < 0) {
        log_platform_error("select(2) failed");
        return RTP_GENERIC_ERROR;
    } else if (ret == 0) {
        set_bytes(bytes_read, 0);
        return RTP_INTERRUPTED;
    }

	if ((rtp_ret = socket->recv((uint8_t *)buf, (int)buf_len, 0, bytes_read)) != RTP_OK) {
        set_bytes(bytes_read, -1);
        log_platform_error("recv(2) failed");
    }

    return rtp_ret;
}

rtp_error_t uvgrtp::poll::poll(std::vector<std::shared_ptr<uvgrtp::socket>>& sockets, uint8_t *buf, size_t buf_len, int timeout, int *bytes_read)
{
    if (buf == nullptr || buf_len == 0)
        return RTP_INVALID_VALUE;

    if (sockets.size() >= MULTICAST_MAX_PEERS) {
        UVG_LOG_ERROR("Too many participants!");
        return RTP_INVALID_VALUE;
    }

#ifndef _WIN32
    struct pollfd fds[MULTICAST_MAX_PEERS];
    int ret;

    for (size_t i = 0; i < sockets.size(); ++i) {
        fds[i].fd      = sockets.at(i)->get_raw_socket();
        fds[i].events  = POLLIN | POLLERR;
    }

    ret = ::poll(fds, sockets.size(), timeout);

    if (ret == -1) {
        set_bytes(bytes_read, -1);
        UVG_LOG_ERROR("Poll failed: %s", strerror(errno));
        return RTP_GENERIC_ERROR;
    }

    if (ret == 0) {
        set_bytes(bytes_read, 0);
        return RTP_INTERRUPTED;
    }

    for (size_t i = 0; i < sockets.size(); ++i) {
        if (fds[i].revents & POLLIN) {
            auto rtp_ret = sockets.at(i)->recv(buf, buf_len, 0, bytes_read);

            if (rtp_ret != RTP_OK) {
                UVG_LOG_ERROR("recv() for socket %d failed: %s", fds[i].fd, strerror(errno));
                set_bytes(bytes_read, -1);
                return RTP_GENERIC_ERROR;
            }
            return RTP_OK;
        }
    }

    /* code should not get here */
    return RTP_GENERIC_ERROR;
#else
    fd_set read_fds;
    struct timeval t_val;

    FD_ZERO(&read_fds);

    for (size_t i = 0; i < sockets.size(); ++i) {
        auto fd = sockets.at(i)->get_raw_socket();
        FD_SET(fd, &read_fds);
    }

    t_val.tv_sec  = timeout / 1000;
    t_val.tv_usec = (timeout % 1000) * 1000;

    int ret = ::select((int)sockets.size(), &read_fds, nullptr, nullptr, &t_val);

    if (ret < 0) {
        log_platform_error("select(2) failed");
        return RTP_GENERIC_ERROR;
    } else if (ret == 0) {
        set_bytes(bytes_read, 0);
        return RTP_INTERRUPTED;
    }

    for (size_t i = 0; i < sockets.size(); ++i) {
        auto rtp_ret = sockets.at(i)->recv((uint8_t *)buf, (int)buf_len, 0, bytes_read);

        if (rtp_ret != RTP_OK) {
            if (WSAGetLastError() == WSAEWOULDBLOCK)
                continue;
        } else {
            return RTP_OK;
        }
    }

    set_bytes(bytes_read, -1);
    return RTP_GENERIC_ERROR;
#endif
}
