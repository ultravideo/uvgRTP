#pragma once

#include <thread>

#include "util.hh"

namespace kvz_rtp {
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
};
