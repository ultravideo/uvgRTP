#pragma once

#include "uvgrtp/util.hh"

#ifndef _WIN32
#include <netinet/in.h>
#endif

#include <memory>

namespace uvgrtp {

    class socket;

    namespace zrtp_msg {

        /* NOTE: This class was deprecated when socket multiplexing was introduced.
         * All reception is done in reception_flow class */
        class receiver {
            public:
                receiver();
                ~receiver();

                /* Listen to "socket" for incomming ZRTP messages
                 *
                 * Return message type on success (see src/frame.hh)
                 * Return -EAGAIN if recv() timed out 
                 * Return -EINVAL if the message received was invalid somehow
                 * Return -EPROTONOSUPPORT if message contains incompatible version number
                 * Return -ENOPNOTSUPP if message type is not supported
                 * Return -errno for any other error */
                rtp_error_t recv_msg(std::shared_ptr<uvgrtp::socket> socket, int timeout, int recv_flags, 
                    int& out_type);

                // get received message to ptr. Len is the size of buffer in ptr.
                ssize_t get_msg(void *ptr, size_t len);

                /* ZRTP packet handler is used after ZRTP state initialization has finished
                 * and media exchange has started. RTP reception flow gives the packet
                 * to "zrtp_handler" which then checks whether the packet is a ZRTP packet
                 * or not and processes it accordingly.
                 *
                 * Return RTP_OK on success
                 * Return RTP_PKT_NOT_HANDLED if "buffer" does not contain a ZRTP message
                 * Return RTP_GENERIC_ERROR if "buffer" contains an invalid ZRTP message */
                rtp_error_t zrtp_handler(ssize_t size, void *buffer);

            private:
                uint8_t *mem_;
                size_t len_;
                size_t rlen_;
        };
    }
}

namespace uvg_rtp = uvgrtp;
