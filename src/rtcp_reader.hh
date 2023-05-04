#pragma once

#include "uvgrtp/clock.hh"
#include "uvgrtp/util.hh"
#include "uvgrtp/frame.hh"

#ifdef _WIN32
#include <ws2ipdef.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#endif

#include <vector>
#include <memory>
#include <map>

namespace uvgrtp {
    class socketfactory;
    class rtcp;
    class socket;

    class rtcp_reader {

        public: 
            rtcp_reader(std::shared_ptr<uvgrtp::socketfactory> sfp);
            ~rtcp_reader();
            void rtcp_report_reader();
            bool set_socket(std::shared_ptr<uvgrtp::socket> socket);

        private:
            bool active_;
            std::shared_ptr<uvgrtp::socketfactory> sfp_;
            std::shared_ptr<uvgrtp::socket> socket_;
            std::map<uint32_t, std::shared_ptr<uvgrtp::rtcp>> rtcps_map_;
    };


}