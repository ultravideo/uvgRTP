#pragma once

#include <map>
#include "session.hh"

namespace kvz_rtp {
    
    class context {
        public:
            context();
            ~context();

            kvz_rtp::session *create_session(std::string addr);

            rtp_error_t destroy_session(kvz_rtp::session *session);

            std::string& get_cname();

        private:
            /* Generate CNAME for participant using host and login names */
            std::string generate_cname();

            /* CNAME is the same for all connections */
            std::string cname_;

            /* std::map<uint32_t, connection *> conns_; */
        };
};
