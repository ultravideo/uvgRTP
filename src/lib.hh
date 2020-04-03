#pragma once

#include <map>
#include "session.hh"

namespace kvz_rtp {
    
    class context {
        public:
            context();
            ~context();

            /* Create a new session with remote participant */
            kvz_rtp::session *create_session(std::string addr);

            /* Create a new session with remote participant
             * Bind ourselves to interface pointed to by "local_addr" */
            kvz_rtp::session *create_session(std::string remote_addr, std::string local_addr);

            /* Destroy session and all media streams
             *
             * Return RTP_INVALID_VALUE if "session" is nullptr
             * Return RTP_NOT_FOUND if "session" has not been allocated from this context */
            rtp_error_t destroy_session(kvz_rtp::session *session);

            std::string& get_cname();

        private:
            /* Generate CNAME for participant using host and login names */
            std::string generate_cname();

            /* CNAME is the same for all connections */
            std::string cname_;
        };
};
