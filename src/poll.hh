#pragma once

#include "uvgrtp/util.hh"

#include <vector>
#include <memory>

namespace uvgrtp {
    class socket;

    namespace poll {
        /* Cross-platform poll implementation for listening to a socket for a period of time
         *
         * This is used by RTCP to listen to socket RTCP_MIN_TIMEOUT (5s for now).
         *
         * "timeout" is in milliseconds
         *
         * If some actions happens with the socket, return status
         * If the timeout is exceeded, return RTP_INTERRUPTED */
        rtp_error_t poll(std::vector<std::shared_ptr<uvgrtp::socket>>& sockets, uint8_t *buf, size_t buf_len, int timeout, int *bytes_read);

        /* TODO:  */
        rtp_error_t blocked_recv(std::shared_ptr<uvgrtp::socket> socket, uint8_t *buf, size_t buf_len, int timeout, int *bytes_read);
    }
}

namespace uvg_rtp = uvgrtp;
