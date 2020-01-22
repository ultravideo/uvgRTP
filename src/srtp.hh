#pragma once

#ifdef _WIN32
#include <winsock2.h>
#include <mswsock.h>
#include <inaddr.h>
#else
#include <netinet/ip.h>
#include <arpa/inet.h>
#endif

#include <cstdint>

#include "debug.hh"
#include "util.hh"
#include "zrtp.hh"

namespace kvz_rtp {

    enum STYPE {
        SRTP  = 0,
        SRTCP = 1
    };

    struct srtp_ctx {
    };

    struct srtcp_ctx {
    };

    struct secure_context {
        int type; /* srtp or srtcp */
    };

    class srtp {
        public:
            srtp(int type);
            ~srtp();

            /* Initialize SRTP state using ZRTP
             * Socket is needed to exchange keys using ZRTP
             * 
             * After this call, encrypt()/decrypt() functions can be called
             *
             * Return RTP_OK on success
             * Return RTP_NOT_SUPPORTED if remote does not support ZRTP
             * Return RTP_MEMORY_ERROR if allocation failed
             * Return RTP_INIT_ERROR if ZRTP has already been initialized for this socket
             * Return RTP_GENERIC_ERROR for any other error */
            rtp_error_t init_zrtp(uint32_t ssrc, socket_t& socket, sockaddr_in& addr);

            /* Initialize SRTP state using user-managed key
             *
             * Parameter "key" is the master key from which all encryption,
             * authentication, and salt keys are derived
             * 
             * After this call, encrypt()/decrypt() functions can be called
             *
             * Return RTP_OK on success
             * Return RTP_NOT_SUPPORTED if remote does support SRTP (TODO: is this true?)
             * Return RTP_MEMORY_ERROR if allocation failed
             * Return RTP_GENERIC_ERROR for any other error */
            rtp_error_t init_user(uint32_t ssrc, std::pair<uint8_t *, size_t>& key);

            /* TODO:  */
            rtp_error_t encrypt(uint8_t *buf, size_t len);

            /* TODO:  */
            rtp_error_t decrypt(uint8_t *buf, size_t len);

        private:
            kvz_rtp::zrtp *zrtp_;
            secure_context *s_ctx;
    };
};
