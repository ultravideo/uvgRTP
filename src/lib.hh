#pragma once

#include <map>
#include "session.hh"

namespace kvz_rtp {
    
    class context {
        public:
            context();
            ~context();

            /* Create new session if "addr" is unique or return pointer to previously created session */
            kvz_rtp::session *create_session(std::string addr);

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
