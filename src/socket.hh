#pragma once

#include "uvgrtp/util.hh"

#ifdef _WIN32
#include <Windows.h>
#include <winsock2.h>
#include <mswsock.h>
#include <inaddr.h>
#include <ws2ipdef.h>
#include <WS2tcpip.h>
#else
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <sys/uio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#endif

#include <vector>
#include <string>
#include <atomic>
#include <memory>
#include <mutex>
#include <map>

#ifdef _WIN32
typedef SOCKET socket_t;
#else
typedef int socket_t;
#endif

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
        return int(n);
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
            socket(int rce_flags);
            ~socket();

            /* Create socket using "family", "type" and "protocol"
             *
             *
             * Return RTP_OK on success
             * return RTP_SOCKET_ERROR if creating the socket failed */
            rtp_error_t init(short family, int type, int protocol);

            /* Same as bind(2), assigns an address for the underlying socket object
             *
             * Return RTP_OK on success
             * Return RTP_BIND_ERROR if the bind failed */
            rtp_error_t bind(short family, unsigned host, short port);
            rtp_error_t bind(sockaddr_in& local_address);
            rtp_error_t bind_ip6(sockaddr_in6& local_address);

            /* Check if the given address is IPv4 or IPv6
             *
             * Return 1 for IPv4
             * Return 2 for IPv6
             * return -1 for error */
            int check_family(std::string addr);

            /* Same as setsockopt(2), used to manipulate the underlying socket object
             *
             * Return RTP_OK on success
             * Return RTP_GENERIC_ERROR if setsockopt failed */
            rtp_error_t setsockopt(int level, int optname, const void *optval, socklen_t optlen);

            /* Same as send(2), send message to the given address with send_flags
             * It is required to give at least one type of address: sockaddr_in or sockaddr_in6. It is possible
             * to give both and the function wil pick the correct one to use
             * 
             * It is possible to combine multiple buffers and send them as one RTP frame by calling
             * the sendto() with a vector containing the buffers and their lengths
             *
             * Write the amount of bytes sent to "bytes_sent" if it's not NULL
             *
             * Return RTP_OK on success and write the amount of bytes sent to "bytes_sent"
             * Return RTP_SEND_ERROR on error and set "bytes_sent" to -1 */

            rtp_error_t sendto(sockaddr_in& addr, sockaddr_in6& addr6, uint8_t *buf, size_t buf_len, int send_flags);
            rtp_error_t sendto(sockaddr_in& addr, sockaddr_in6& addr6, uint8_t *buf, size_t buf_len, int send_flags, int *bytes_sent);
            rtp_error_t sendto(sockaddr_in& addr, sockaddr_in6& addr6, buf_vec& buffers, int send_flags);
            rtp_error_t sendto(sockaddr_in& addr, sockaddr_in6& addr6, buf_vec& buffers, int send_flags, int *bytes_sent);
            rtp_error_t sendto(sockaddr_in& addr, sockaddr_in6& addr6, pkt_vec& buffers, int send_flags);
            rtp_error_t sendto(sockaddr_in& addr, sockaddr_in6& addr6, pkt_vec& buffers, int send_flags, int *bytes_sent);

            /* Same as recv(2), receives a message from socket (remote address not known)
             *
             * Write the amount of bytes read to "bytes_read" if it's not NULL
             *
             * Return RTP_OK on success and write the amount of bytes received to "bytes_read"
             * Return RTP_INTERRUPTED if the call was interrupted due to timeout and set "bytes_sent" to 0
             * Return RTP_GENERIC_ERROR on error and set "bytes_sent" to -1 */
            rtp_error_t recv(uint8_t *buf, size_t buf_len, int recv_flags);
            rtp_error_t recv(uint8_t *buf, size_t buf_len, int recv_flags, int *bytes_read);

            /* Same as recvfrom(2), receives a message from remote
             *
             * Write the sender address to "sender" if it's not NULL
             * Write the amount of bytes read to "bytes_read" if it's not NULL
             *
             * Return RTP_OK on success and write the amount of bytes sent to "bytes_sent"
             * Return RTP_INTERRUPTED if the call was interrupted due to timeout and set "bytes_sent" to 0
             * Return RTP_GENERIC_ERROR on error and set "bytes_sent" to -1 */
            rtp_error_t recvfrom(uint8_t *buf, size_t buf_len, int recv_flags, sockaddr_in *sender,
                sockaddr_in6 *sender6, int *bytes_read);
            rtp_error_t recvfrom(uint8_t *buf, size_t buf_len, int recv_flags, sockaddr_in *sender);
            rtp_error_t recvfrom(uint8_t *buf, size_t buf_len, int recv_flags, int *bytes_read);
            rtp_error_t recvfrom(uint8_t *buf, size_t buf_len, int recv_flags);

            /* Create sockaddr_in (IPv4) object using the provided information
             * NOTE: "family" must be AF_INET */
            static sockaddr_in create_sockaddr(short family, unsigned host, short port);

            /* Create sockaddr_in object using the provided information
             * NOTE: "family" must be AF_INET */
            static sockaddr_in create_sockaddr(short family, std::string host, short port);

            /* Create sockaddr_in6 (IPv6) object using the provided information */
            static sockaddr_in6 create_ip6_sockaddr(unsigned host, short port);
            static sockaddr_in6 create_ip6_sockaddr(std::string host, short port);
            static sockaddr_in6 create_ip6_sockaddr_any(short src_port);


            std::string get_socket_path_string() const;

            static std::string sockaddr_to_string(const sockaddr_in& addr);
            static std::string sockaddr_ip6_to_string(const sockaddr_in6& addr6);

            /* Get reference to the actual socket object */
            socket_t& get_raw_socket();

            /* Install a packet handler for vector-based send operations.
             *
             * This handler allows the caller to inject extra functionality to the send operation
             * without polluting src/socket.cc with unrelated code
             * (such as collecting RTCP session statistics info or encrypting a packet)
             *
             * "arg" is an optional parameter that can be passed to the handler when it's called */
            rtp_error_t install_handler(std::shared_ptr<std::atomic<std::uint32_t>> local_ssrc, void *arg, packet_handler_vec handler);

            rtp_error_t remove_handler(std::shared_ptr<std::atomic<std::uint32_t>> local_ssrc);

            static bool is_multicast(sockaddr_in& local_address);
            static bool is_multicast(sockaddr_in6& local_address);


        private:

            /* helper function for sending UPD packets, see documentation for sendto() above */
            rtp_error_t __sendto(sockaddr_in& addr, sockaddr_in6& addr6, bool ipv6, uint8_t *buf, size_t buf_len, int send_flags, int *bytes_sent);

            rtp_error_t __recv(uint8_t *buf, size_t buf_len, int recv_flags, int *bytes_read);

            rtp_error_t __recvfrom_ip6(uint8_t* buf, size_t buf_len, int recv_flags, sockaddr_in6* sender, int* bytes_read);
            rtp_error_t __recvfrom(uint8_t *buf, size_t buf_len, int recv_flags, sockaddr_in *sender, int *bytes_read);

            /* __sendtov() does the same as __sendto but it combines multiple buffers into one frame and sends them */
            rtp_error_t __sendtov(sockaddr_in& addr, sockaddr_in6& addr6, bool ipv6, buf_vec& buffers, int send_flags, int *bytes_sent);
            rtp_error_t __sendtov(sockaddr_in& addr, sockaddr_in6& addr6, bool ipv6, uvgrtp::pkt_vec& buffers, int send_flags, int *bytes_sent);

            socket_t socket_;
            //sockaddr_in remote_address_;
            sockaddr_in local_address_;
            //sockaddr_in6 remote_ip6_address_;
            sockaddr_in6 local_ip6_address_;
            bool ipv6_;

            int rce_flags_;

            std::mutex handlers_mutex_;
            std::mutex conf_mutex_;

            /* __sendto() calls these handlers in order before sending the packet */
            std::multimap<std::shared_ptr<std::atomic<std::uint32_t>>, socket_packet_handler> buf_handlers_;

            /* __sendtov() calls these handlers in order before sending the packet */
            std::multimap<std::shared_ptr<std::atomic<std::uint32_t>>, socket_packet_handler> vec_handlers_;

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
