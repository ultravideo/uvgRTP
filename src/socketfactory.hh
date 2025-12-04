#pragma once

#include "uvgrtp/util.hh"
#include "reception_flow.hh"
#include <string>
#include <memory>
#include <vector>
#include <functional>
#include <map>

namespace uvgrtp {

    class socket;
    class reception_flow;
    class rtcp_reader;

    /* This class keeps track of all the sockets that uvgRTP is using. 
     * Each socket will have either a reception_flow or an rtcp_reader depending on what the socket
     * is used for. It is possible to multiplex several media streams into a single socket, and the 
     * reception_flow of the socket will distribute packets to the correct receiving stream based on the
     * SSRCs on the packets. 
     * *Attention* : If you multiplex multiple media streams into a single socket, you *must* set their 
     * REMOTE SSRCs via the RCC_REMOTE_SSRC context flag.
     * The reception_flow looks at the SOURCE SSRC on the received packet and then checks, which local
     * stream is supposed to receive packets from this remote source.
     * If you have a separate socket for every stream, this does not need to be taken into account
     * This is also true for RTCP: the rtcp_reader will distribute received packets depending on the 
     * SSRCs in the packets
     */

    class socketfactory {

        public:
            socketfactory();
            ~socketfactory();

            /* Set the local addres for socketfactory.
             * 
             * Param local_addr local IPv4 or IPv6 address
             * Return RTP OK on success */
            rtp_error_t set_local_interface(std::string local_addr);

            /* Create a new socket. Depending on if the local address was IPv4 or IPv6, the socket
             * will use the correct IP version
             *
             * Param type 1 RTCP socket, 2 for any other type of a socket
             * Return the created socket on success, nullptr otherwise */
            /* Create a new socket. Parameters:
             *  - type: 1 = RTCP socket, 2 = other (media)
             *  - port: port to bind into (0 = none)
             *  - rce_flags: optional flags propagated to the socket instance
             */
            std::shared_ptr<uvgrtp::socket> create_new_socket(int type, uint16_t port, int rce_flags = 0);

            /* Bind socket to the local IP address and given port
             * 
             * Param soc pointer to socket
             * Param port port to bind into
             * Return RTP OK on success */
            rtp_error_t bind_socket(std::shared_ptr<uvgrtp::socket> soc, uint16_t port);

            /* Bind socket any address and given port
             *
             * Param soc pointer to socket
             * Param port port to bind into
             * Return RTP OK on success */
            rtp_error_t bind_socket_anyip(std::shared_ptr<uvgrtp::socket> soc, uint16_t port);

            /* Get the socket bound to the given port
             *
             * Param port socket with wanted port
             * Return pointer to socket on success. If one does not exist, a new one is created */
            /* Get or create socket for a given port. `rce_flags` is forwarded
             * to `create_new_socket` when a new socket is created. */
            std::shared_ptr<uvgrtp::socket> get_socket_ptr(int type, uint16_t port, int rce_flags = 0);

            /* Get reception flow matching the given socket
             *
             * Param socket socket matching the wanted reception_flow
             * Return pointer to reception_flow on success, nullptr otherwise */
            std::shared_ptr<uvgrtp::reception_flow> get_reception_flow_ptr(std::shared_ptr<uvgrtp::socket> socket);
            
            /* Install an RTCP reader and map it to the given port
             *
             * Param port port to map the created RTCP reader into
             * Return pointer to created RTCP reader */
            std::shared_ptr<uvgrtp::rtcp_reader> install_rtcp_reader(uint16_t port);

            /* Get a pointer to the RTCP reader matching the given port
             *
             * Param port into which the wanted RTCP reader is mapped into
             * Return pointer to RTCP reader */
            std::shared_ptr <uvgrtp::rtcp_reader> get_rtcp_reader(uint16_t port);

            /* Clear all receiver modules from the given socket + port combo
             *
             * Param socket socket to be cleared
             * Param port port to be cleared
             * Param flow that will be cleared from the socket
             * true on success */
            bool clear_port(uint16_t port, std::shared_ptr<uvgrtp::socket> socket);

            /// \cond DO_NOT_DOCUMENT
            bool get_ipv6() const;
            bool is_port_in_use(uint16_t port);
            /// \endcond

        private:

            // protect maps
            std::mutex conf_mutex_;

            // local address used for binds; empty means any-address
            std::string local_address_;
            bool ipv6_address_;

            // port -> socket object
            std::map<uint16_t, std::shared_ptr<uvgrtp::socket>> port_to_socket_;

            // reception flow -> its socket (used when multiplexing multiple streams)
            std::map<std::shared_ptr<uvgrtp::reception_flow>, std::shared_ptr<uvgrtp::socket>> reception_flows_;

            // rtcp reader -> bound port
            std::map<std::shared_ptr<uvgrtp::rtcp_reader>, uint16_t> rtcp_readers_to_ports_;

    };
}