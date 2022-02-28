#pragma once

#include "util.hh"

#include <thread>

namespace uvgrtp {
    class runner {
        public:
            runner();
            virtual ~runner();

            virtual rtp_error_t start();
            virtual rtp_error_t stop();
            
            virtual bool active();

        protected:
            bool active_;
            std::thread *runner_;
    };
}

namespace uvg_rtp = uvgrtp;
