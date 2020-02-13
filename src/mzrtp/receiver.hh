#ifdef __RTP_CRYPTO__
#pragma once

namespace kvz_rtp {
    namespace zrtp_msg {

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
                int recv_msg(socket_t& socket, int flags);

                /* TODO:  */
                ssize_t get_msg(void *ptr, size_t len);

            private:
                uint8_t *mem_;
                size_t len_;
                size_t rlen_;
        };
    };
};
#endif
