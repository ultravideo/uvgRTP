#pragma once

#include <chrono>

namespace kvz_rtp {
    namespace clock {
        typedef std::chrono::high_resolution_clock::time_point tp_t;

        uint64_t now_ntp();
        uint32_t now_epoch();
        uint32_t now_epoch_ms();
        tp_t     now_high_res();

        uint64_t diff_ntp_ms(uint64_t ntp1, uint64_t ntp2);
        uint64_t diff_tp_ms(tp_t t1, tp_t t2);
        uint64_t diff_tp_s(tp_t t1, tp_t t2);

        uint64_t diff_ntp_now_ms(uint64_t then);

        /* Calculate the difference between now and then 
         * (now - then) and return the result in milliseconds */
        uint64_t diff_tp_now_ms(tp_t then);
        uint64_t diff_tp_now_s(tp_t then);

        uint64_t ms_to_jiffies(uint64_t ms);
        uint64_t jiffies_to_ms(uint64_t jiffies);
    };
};
