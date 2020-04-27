#pragma once

#ifdef _WIN32
#include <winsock2.h>
#include <mswsock.h>
#include <inaddr.h>
#else
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <sys/uio.h>
#endif

#include <vector>
#include <string>

#include "srtp.hh"
#include "util.hh"

namespace uvg_rtp {

#ifdef _WIN32
    typedef unsigned socklen_t;
    typedef TRANSMIT_PACKETS_ELEMENT vecio_buf;
#else
    typedef mmsghdr vecio_buf;
#endif

    const int MAX_BUFFER_COUNT = 256;

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
             * It is possible to combine multiple buffers and send them as one RTP frame by calling
             * the sendto() with a vector containing the buffers and their lengths
             *
             * Write the amount of bytes sent to "bytes_sent" if it's not NULL
             *
             * Return RTP_OK on success and write the amount of bytes sent to "bytes_sent"
             * Return RTP_SEND_ERROR on error and set "bytes_sent" to -1 */
            rtp_error_t sendto(uint8_t *buf, size_t buf_len, int flags);
            rtp_error_t sendto(uint8_t *buf, size_t buf_len, int flags, int *bytes_sent);
            rtp_error_t sendto(std::vector<std::pair<size_t, uint8_t *>> buffers, int flags);
            rtp_error_t sendto(std::vector<std::pair<size_t, uint8_t *>> buffers, int flags, int *bytes_sent);

            /* Same as sendto() but the remote address given as parameter */
            rtp_error_t sendto(sockaddr_in& addr, uint8_t *buf, size_t buf_len, int flags);
            rtp_error_t sendto(sockaddr_in& addr, uint8_t *buf, size_t buf_len, int flags, int *bytes_sent);
            rtp_error_t sendto(sockaddr_in& addr, std::vector<std::pair<size_t, uint8_t *>> buffers, int flags);
            rtp_error_t sendto(sockaddr_in& addr, std::vector<std::pair<size_t, uint8_t *>> buffers, int flags, int *bytes_sent);

            /* Special sendto() function, used to send multiple UDP packets with one system call
             * Internally it uses sendmmsg(2) (Linux) or TransmitPackets (Windows)
             *
             * Return RTP_OK on success
             * Return RTP_INVALID_VALUE if one of the parameters is invalid
             * Return RTP_SEND_ERROR if sendmmsg/TransmitPackets failed */
            rtp_error_t send_vecio(vecio_buf *buffers, size_t nbuffers, int flags);

            /* Special recv() function, used to receive multiple UDP packets with one system call
             * Internally it uses recvmmsg(2) (Linux).
             *
             * TODO what about windows?
             *
             * Parameter "nread" is used to indicate how many packet were read from the OS.
             * It may differ from "nbuffers"
             *
             * Return RTP_OK on success
             * Return RTP_INVALID_VALUE if one of the parameters is invalid
             * Return RTP_SEND_ERROR if sendmmsg/TransmitPackets failed */
            rtp_error_t recv_vecio(vecio_buf *buffers, size_t nbuffers, int flags, int *nread);

            /* Same as recv(2), receives a message from socket (remote address not known)
             *
             * Write the amount of bytes read to "bytes_read" if it's not NULL
             *
             * Return RTP_OK on success and write the amount of bytes received to "bytes_read"
             * Return RTP_INTERRUPTED if the call was interrupted due to timeout and set "bytes_sent" to 0
             * Return RTP_GENERIC_ERROR on error and set "bytes_sent" to -1 */
            rtp_error_t recv(uint8_t *buf, size_t buf_len, int flags);
            rtp_error_t recv(uint8_t *buf, size_t buf_len, int flags, int *bytes_read);

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

            /* Get reference to the actual socket object */
            socket_t& get_raw_socket();

            /* Initialize the private "addr_" object with "addr"
             * This is used when calling send() */
            void set_sockaddr(sockaddr_in addr);

            /* TODO:  */
            void set_srtp(uvg_rtp::srtp *srtp);

            /* Get the out address for the socket if it exists */
            sockaddr_in& get_out_address();

            /* Some media types (such as HEVC) require finer control over the sending/receiving process
             * These functions can be used to install low-level (the higher level API will call these)
             * functions for dealing with media-specific details
             *
             * Return RTP_OK if installation succeeded
             * Return RTP_INVALID_VALUE if the handler is nullptr */
            void install_ll_recv(rtp_error_t (*recv)(socket_t, uint8_t *, size_t , int , sockaddr_in *, int *));
            void install_ll_sendto(rtp_error_t (*sendto)(socket_t, sockaddr_in&, uint8_t *, size_t, int, int *));
            void install_ll_sendtov(rtp_error_t (*send)(socket_t, sockaddr_in&, std::vector<std::pair<size_t, uint8_t *>>, int, int *));

        private:
            /* helper function for sending UPD packets, see documentation for sendto() above */
            rtp_error_t __sendto(sockaddr_in& addr, uint8_t *buf, size_t buf_len, int flags, int *bytes_sent);
            rtp_error_t __recv(uint8_t *buf, size_t buf_len, int flags, int *bytes_read);
            rtp_error_t __recvfrom(uint8_t *buf, size_t buf_len, int flags, sockaddr_in *sender, int *bytes_read);

            /* __sendtov() does the same as __sendto but it combines multiple buffers into one frame and sends them */
            rtp_error_t __sendtov(sockaddr_in& addr, std::vector<std::pair<size_t, uint8_t *>> buffers, int flags, int *bytes_sent);

            rtp_error_t (*recv_handler_)(socket_t, uint8_t *, size_t , int , sockaddr_in *, int *);
            rtp_error_t (*sendto_handler_)(socket_t, sockaddr_in&, uint8_t *, size_t, int, int *);
            rtp_error_t (*sendtov_handler_)(socket_t, sockaddr_in&, std::vector<std::pair<size_t, uint8_t *>>, int, int *);

            socket_t socket_;
            sockaddr_in addr_;
            uvg_rtp::srtp *srtp_;

#ifdef _WIN32
            WSABUF buffers_[MAX_BUFFER_COUNT];
#else
            struct mmsghdr header_;
            struct iovec   chunks_[MAX_BUFFER_COUNT];
#endif
    };
};
