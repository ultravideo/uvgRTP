#pragma once

#include "util.hh"

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


namespace uvgrtp {

#ifdef _WIN32
    typedef unsigned int socklen_t;
#endif

#if defined(UVGRTP_HAVE_SENDMSG) && !defined(UVGRTP_HAVE_SENDMMSG)
    struct mmsghdr {
        struct msghdr msg_hdr;
        unsigned int msg_len;
    };
    static inline
    int sendmmsg(int sockfd, struct mmsghdr *msgvec, unsigned int vlen,
        int flags)
    {
        ssize_t n = 0;
        for (unsigned int i = 0; i < vlen; i++) {
            ssize_t ret = sendmsg(sockfd, &msgvec[i].msg_hdr, flags);
            if (ret < 0)
                break;
            n += ret;
        }
        if (n == 0)
            return -1;
        return n;
    }
#endif

    const int MAX_BUFFER_COUNT = 256;

    /* Vector of buffers that contain a full RTP frame */
    typedef std::vector<std::pair<size_t, uint8_t *>> buf_vec;

    /* Vector of RTP frames constructed from buf_vec entries */
    typedef std::vector<std::vector<std::pair<size_t, uint8_t *>>> pkt_vec;

    typedef rtp_error_t (*packet_handler_vec)(void *, buf_vec&);

    struct socket_packet_handler {
        void *arg = nullptr;
        packet_handler_vec handler = nullptr;
    };

    class socket {
        public:
            socket(int flags);
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
            rtp_error_t sendto(buf_vec& buffers, int flags);
            rtp_error_t sendto(buf_vec& buffers, int flags, int *bytes_sent);
            rtp_error_t sendto(pkt_vec& buffers, int flags);
            rtp_error_t sendto(pkt_vec& buffers, int flags, int *bytes_sent);

            /* Same as sendto() but the remote address given as parameter */
            rtp_error_t sendto(sockaddr_in& addr, uint8_t *buf, size_t buf_len, int flags);
            rtp_error_t sendto(sockaddr_in& addr, uint8_t *buf, size_t buf_len, int flags, int *bytes_sent);
            rtp_error_t sendto(sockaddr_in& addr, buf_vec& buffers, int flags);
            rtp_error_t sendto(sockaddr_in& addr, buf_vec& buffers, int flags, int *bytes_sent);
            rtp_error_t sendto(sockaddr_in& addr, pkt_vec& buffers, int flags);
            rtp_error_t sendto(sockaddr_in& addr, pkt_vec& buffers, int flags, int *bytes_sent);

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

            /* Get the out address for the socket if it exists */
            sockaddr_in& get_out_address();

            /* Install a packet handler for vector-based send operations.
             *
             * This handler allows the caller to inject extra functionality to the send operation
             * without polluting src/socket.cc with unrelated code
             * (such as collecting RTCP session statistics info or encrypting a packet)
             *
             * "arg" is an optional parameter that can be passed to the handler when it's called */
            rtp_error_t install_handler(void *arg, packet_handler_vec handler);

        private:
            /* helper function for sending UPD packets, see documentation for sendto() above */
            rtp_error_t __sendto(sockaddr_in& addr, uint8_t *buf, size_t buf_len, int flags, int *bytes_sent);
            rtp_error_t __recv(uint8_t *buf, size_t buf_len, int flags, int *bytes_read);
            rtp_error_t __recvfrom(uint8_t *buf, size_t buf_len, int flags, sockaddr_in *sender, int *bytes_read);

            /* __sendtov() does the same as __sendto but it combines multiple buffers into one frame and sends them */
            rtp_error_t __sendtov(sockaddr_in& addr, buf_vec& buffers, int flags, int *bytes_sent);
            rtp_error_t __sendtov(sockaddr_in& addr, uvgrtp::pkt_vec& buffers, int flags, int *bytes_sent);

            socket_t socket_;
            sockaddr_in addr_;
            int flags_;

            /* __sendto() calls these handlers in order before sending the packet */
            std::vector<socket_packet_handler> buf_handlers_;

            /* __sendtov() calls these handlers in order before sending the packet */
            std::vector<socket_packet_handler> vec_handlers_;

#ifndef NDEBUG
            uint64_t sent_packets_ = 0;
            uint64_t received_packets_ = 0;
#endif // !NDEBUG

#ifdef _WIN32
            WSABUF buffers_[MAX_BUFFER_COUNT];
#else
            struct mmsghdr header_;
            struct iovec   chunks_[MAX_BUFFER_COUNT];
#endif
    };
}

namespace uvg_rtp = uvgrtp;
