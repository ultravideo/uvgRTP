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

    class socketfactory {

        public:
            socketfactory(int rce_flags);
            ~socketfactory();
            rtp_error_t stop();

            rtp_error_t set_local_interface(std::string local_addr);
            std::shared_ptr<uvgrtp::socket> create_new_socket();
            rtp_error_t bind_socket(std::shared_ptr<uvgrtp::socket> soc, uint16_t port);
            rtp_error_t bind_socket_anyip(std::shared_ptr<uvgrtp::socket> soc, uint16_t port);

            std::shared_ptr<uvgrtp::reception_flow> install_reception_flow(std::shared_ptr<uvgrtp::socket> socket);
            std::shared_ptr<uvgrtp::socket> get_socket_ptr(uint16_t port) const;
            std::shared_ptr<uvgrtp::reception_flow> get_reception_flow_ptr(std::shared_ptr<uvgrtp::socket> socket) const;
            rtp_error_t map_port_to_rtcp_reader(uint16_t port, std::shared_ptr <uvgrtp::rtcp_reader> reader);
            std::shared_ptr <uvgrtp::rtcp_reader> get_rtcp_reader(uint16_t port);

            bool get_ipv6() const;
            bool is_port_in_use(uint16_t port) const;

        private:
            
            std::mutex socket_mutex_;

            int rce_flags_;
            std::string local_address_;
            std::map<uint16_t, std::shared_ptr<uvgrtp::socket>> used_ports_;
            bool ipv6_;
            std::vector<std::shared_ptr<uvgrtp::socket>> used_sockets_;
            std::map<std::shared_ptr<uvgrtp::reception_flow>, std::shared_ptr<uvgrtp::socket>> reception_flows_;
            std::map<std::shared_ptr<uvgrtp::rtcp_reader>, uint16_t> rtcp_readers_to_ports_;

            bool should_stop_;


    };
}