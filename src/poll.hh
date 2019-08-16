#pragma once

#include <vector>

#include "socket.hh"

namespace kvz_rtp {
    namespace poll {
        /* Cross-platform poll implementation for listening to a socket for a period of time
         *
         * This is used by RTCP to listen to socket RTCP_MIN_TIMEOUT (5s for now).
         *
         * "timeout" is in milliseconds
         *
         * If some actions happens with the socket, return status
         * If the timeout is exceeded, return RTP_INTERRUPTED */
        rtp_error_t poll(std::vector<kvz_rtp::socket>& sockets, uint8_t *buf, size_t buf_len, int timeout, int *bytes_read);
    };
};
