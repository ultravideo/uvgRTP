#pragma once

#ifdef _WIN32
#include <inaddr.h>
#include <winsock2.h>
#else
#include <netinet/ip.h>
#include <arpa/inet.h>
#endif

#include <vector>
#include <string>

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

            /* Create socket using "family", "type" and "protocol"
             *
             * NOTE: Only family AF_INET (ie. IPv4) is supported
             *
             * Return RTP_OK on success
             * return RTP_SOCKET_ERROR if creating the socket failed */
            rtp_error_t init(short family, int type, int protocol);

            /* Same as bind(2), assigns an address for the underlying socket object
             *
             * Return RTP_OK on success
             * Return RTP_BIND_ERROR if the bind failed */
            rtp_error_t bind(short family, unsigned host, short port);

            /* Same as setsockopt(2), used to manipulate the underlying socket object
             *
             * Return RTP_OK on success
             * Return RTP_GENERIC_ERROR if setsockopt failed */
            rtp_error_t setsockopt(int level, int optname, const void *optval, socklen_t optlen);

            /* Same as send(2), send message to remote using "flags"
             * This function uses the internal addr_ object as remote address so it MUST be set
             *
             * Write the amount of bytes sent to "bytes_sent" if it's not NULL
             *
             * Return RTP_OK on success and write the amount of bytes sent to "bytes_sent"
             * Return RTP_SEND_ERROR on error and set "bytes_sent" to -1 */
            rtp_error_t sendto(uint8_t *buf, size_t buf_len, int flags);
            rtp_error_t sendto(uint8_t *buf, size_t buf_len, int flags, int *bytes_sent);

            /* Same as sendto() but the remote address given as parameter */
            rtp_error_t sendto(sockaddr_in& addr, uint8_t *buf, size_t buf_len, int flags);
            rtp_error_t sendto(sockaddr_in& addr, uint8_t *buf, size_t buf_len, int flags, int *bytes_sent);

            /* Same as recvfrom(2), receives a message from remote
             *
             * Write the sender address to "sender" if it's not NULL
             * Write the amount of bytes read to "bytes_read" if it's not NULL
             *
             * Return RTP_OK on success and write the amount of bytes sent to "bytes_sent"
             * Return RTP_INTERRUPTED if the call was interrupted due to timeout and set "bytes_sent" to 0
             * Return RTP_GENERIC_ERROR on error and set "bytes_sent" to -1 */
            rtp_error_t recvfrom(uint8_t *buf, size_t buf_len, int flags, sockaddr_in *sender, int *bytes_read);
            rtp_error_t recvfrom(uint8_t *buf, size_t buf_len, int flags, sockaddr_in *sender);
            rtp_error_t recvfrom(uint8_t *buf, size_t buf_len, int flags, int *bytes_read);
            rtp_error_t recvfrom(uint8_t *buf, size_t buf_len, int flags);

            /* Create sockaddr_in object using the provided information
             * NOTE: "family" must be AF_INET */
            sockaddr_in create_sockaddr(short family, unsigned host, short port);

            /* Create sockaddr_in object using the provided information
             * NOTE: "family" must be AF_INET */
            sockaddr_in create_sockaddr(short family, std::string host, short port);

            /* Get const reference to the actual socket object */
            const socket_t& get_raw_socket() const;

            /* Initialize the private "addr_" object with "addr"
             * This is used when calling send() */
            void set_sockaddr(sockaddr_in addr);

        private:
            /* helper function for sending UPD packets, see documentation for sendto() above */
            rtp_error_t __sendto(sockaddr_in& addr, uint8_t *buf, size_t buf_len, int flags, int *bytes_sent);
            rtp_error_t __recvfrom(uint8_t *buf, size_t buf_len, int flags, sockaddr_in *sender, int *bytes_read);

            socket_t socket_;
            sockaddr_in addr_;
    };
};
