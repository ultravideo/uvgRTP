#ifndef __RTP_HH_
#define __RTP_HH_

#include "clock.hh"
#include "util.hh"

namespace uvg_rtp {

    class rtp {
        public:
            rtp(rtp_format_t fmt);
            ~rtp();

            uint32_t get_ssrc();
            uint16_t get_sequence();

            void inc_sent_pkts();
            void inc_sequence();

            void set_clock_rate(size_t rate);
            void set_payload(rtp_format_t fmt);

            void fill_header(uint8_t *buffer);
            void update_sequence(uint8_t *buffer);

        private:
            uint32_t ssrc_;
            uint32_t ts_;
            uint16_t seq_;
            uint8_t fmt_;

            uint32_t clock_rate_;
            uint32_t wc_start_;
            uvg_rtp::clock::hrc::hrc_t wc_start_2;

            size_t sent_pkts_;
    };
};

#endif /* __RTP_HH_ */
