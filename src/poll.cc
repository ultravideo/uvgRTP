#ifdef _WIN32
#else
#include <unistd.h>
#include <poll.h>
#endif

#include <cstring>

#include "debug.hh"
#include "multicast.hh"
#include "poll.hh"

rtp_error_t kvz_rtp::poll::poll(std::vector<kvz_rtp::socket>& sockets, uint8_t *buf, size_t buf_len, int timeout, int *bytes_read)
{
    if (buf == nullptr || buf_len == 0)
        return RTP_INVALID_VALUE;

    if (sockets.size() >= kvz_rtp::MULTICAST_MAX_PEERS) {
        LOG_ERROR("Too many participants!");
        return RTP_INVALID_VALUE;
    }

#ifdef __linux__
    struct pollfd fds[kvz_rtp::MULTICAST_MAX_PEERS];
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
            if ((ret = recv(fds[i].fd, buf, buf_len, 0)) < 0) {
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
#if 0
    WSAPOLLFD fdarray[kvz_rtp::MULTICAST_MAX_PEERS];
    int ret;

    for (size_t i = 0; i < sockets.size(); ++i) {
        fdarray[i].fd = sockets.at(i).get_raw_socket();
        fds[i].events = POLLRDNORM;
    }

    if ((ret = WSAPoll(&fdarray, sockets.size(), DEFAULT_WAIT)) == SOCKET_ERROR) {
        ERR("WSAPoll");
        set_bytes(bytes_read, -1);
        return RTP_GENERIC_ERROR;
    }

    if (ret) {
        for (size_t i = 0; i < sockets.size(); ++i) {
            if (fdarray[i].revents & POLLRDNORM) {
                if ((ret = recv(csock, buf, buf_len, 0)) == SOCKET_ERROR) {
                    ERR("recv");
                    return RTP_GENERIC_ERROR;
                } else {
                    set_bytes(bytes_read, ret);
                    return RTP_OK;
                }
            }
        }
    }

    /* TODO: ?? */
    /* WaitForSingleObject(hCloseSignal, DEFAULT_WAIT); */
#endif
    return RTP_GENERIC_ERROR;
#endif
}
