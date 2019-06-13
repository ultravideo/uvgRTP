#pragma once

#ifdef _WIN32
#include <inaddr.h>
#include <winsock2.h>
#else
#include <netinet/ip.h>
#include <arpa/inet.h>
#endif

#include "util.hh"

namespace kvz_rtp {

#ifdef _WIN32
    typedef SOCKET socket_t;
#else
    typedef int    socket_t;
#endif

    class socket {
        public:
            socket();
            ~socket();

            /* TODO:  */
            rtp_error_t init(short family, int type, int protocol);

            /* TODO:  */
            rtp_error_t bind(short family, unsigned host, short port);

            /* TODO:  */
            rtp_error_t setsockopt(int level, int optname, const void *optval, socklen_t optlen);

            /* TODO:  */
            rtp_error_t sendto(uint8_t *buf, size_t buf_len, int flags, int *bytes_sent);

            /* TODO:  */
            rtp_error_t recvfrom(uint8_t *buf, size_t buf_len, int flags, int *bytes_read);

            /* TODO:  */
            sockaddr_in create_sockaddr(short family, unsigned host,    short port);

            /* TODO:  */
            sockaddr_in create_sockaddr(short family, std::string host, short port);

            /* TODO:  */
            socket_t get_raw_socket() const;

            /* TODO:  */
            void set_sockaddr(sockaddr_in addr);

        private:
            socket_t socket_;
            sockaddr_in addr_;
    };
};
