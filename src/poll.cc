#ifdef _WIN32
#include <winsock2.h>
#else
#include <unistd.h>
#include <poll.h>
#endif

#include <cstring>

#include "debug.hh"
#include "multicast.hh"
#include "poll.hh"

rtp_error_t uvg_rtp::poll::poll(std::vector<uvg_rtp::socket>& sockets, uint8_t *buf, size_t buf_len, int timeout, int *bytes_read)
{
    if (buf == nullptr || buf_len == 0)
        return RTP_INVALID_VALUE;

    if (sockets.size() >= uvg_rtp::MULTICAST_MAX_PEERS) {
        LOG_ERROR("Too many participants!");
        return RTP_INVALID_VALUE;
    }

#ifdef __linux__
    struct pollfd fds[uvg_rtp::MULTICAST_MAX_PEERS];
    int ret;

    for (size_t i = 0; i < sockets.size(); ++i) {
        fds[i].fd      = sockets.at(i).get_raw_socket();
        fds[i].events  = POLLIN | POLLERR;
    }

    ret = ::poll(fds, sockets.size(), timeout);

    if (ret == -1) {
        set_bytes(bytes_read, -1);
        LOG_ERROR("Poll failed: %s", strerror(errno));
        return RTP_GENERIC_ERROR;
    }

    if (ret == 0) {
        set_bytes(bytes_read, 0);
        return RTP_INTERRUPTED;
    }

    for (size_t i = 0; i < sockets.size(); ++i) {
        if (fds[i].revents & POLLIN) {
            auto rtp_ret = sockets.at(i).recv(buf, buf_len, 0);

            if (rtp_ret != RTP_OK) {
                LOG_ERROR("recv() for socket %d failed: %s", fds[i].fd, strerror(errno));
                return RTP_GENERIC_ERROR;
            }

            set_bytes(bytes_read, ret);
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
        auto fd = sockets.at(i).get_raw_socket();
        FD_SET(fd, &read_fds);
    }

    t_val.tv_sec  = timeout / 1000;
    t_val.tv_usec = 0;

    int ret = ::select((int)sockets.size(), &read_fds, nullptr, nullptr, &t_val);

    if (ret < 0) {
        log_platform_error("select(2) failed");
        return RTP_GENERIC_ERROR;
    } else if (ret == 0) {
        set_bytes(bytes_read, 0);
        return RTP_INTERRUPTED;
    }

    for (size_t i = 0; i < sockets.size(); ++i) {
        auto rtp_ret = sockets.at(i).recv((uint8_t *)buf, (int)buf_len, 0);

        if (rtp_ret != RTP_OK) {
            if (WSAGetLastError() == WSAEWOULDBLOCK)
                continue;
        } else {
            set_bytes(bytes_read, ret);
            return RTP_OK;
        }
    }

    set_bytes(bytes_read, -1);
    return RTP_GENERIC_ERROR;
#endif
}
