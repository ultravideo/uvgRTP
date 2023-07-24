#pragma once

#include "uvgrtp/clock.hh"
#include "uvgrtp/util.hh"

#include <chrono>
#include <memory>
#include <atomic>

namespace uvgrtp {

    namespace frame
    {
        struct rtp_frame;
    }

    class rtp {
        public:
            rtp(rtp_format_t fmt, std::shared_ptr<std::atomic<std::uint32_t>> ssrc, bool ipv6);
            ~rtp();

            uint32_t     get_ssrc()          const;
            uint16_t     get_sequence()      const;
            uint32_t     get_clock_rate()    const;
            size_t       get_payload_size()  const;
            size_t       get_pkt_max_delay() const;
            rtp_format_t get_payload()       const;
            uint64_t     get_sampling_ntp()  const;
            uint32_t     get_rtp_ts()        const;

            void inc_sent_pkts();
            void inc_sequence();

            void set_clock_rate(uint32_t rate);

            void set_dynamic_payload(uint8_t payload);
            uint8_t get_dynamic_payload() const;
            void set_timestamp(uint64_t timestamp);
            void set_payload_size(size_t payload_size);
            void set_pkt_max_delay(size_t delay);
            void set_sampling_ntp(uint64_t ntp_ts);

            void fill_header(uint8_t *buffer);
            void update_sequence(uint8_t *buffer);

            /* Validates the RTP header pointed to by "packet" */
            rtp_error_t packet_handler(void* args, int rce_flags, uint8_t* packet, size_t size, uvgrtp::frame::rtp_frame** out);

        private:

            void set_default_clock_rate(rtp_format_t fmt);

            std::shared_ptr<std::atomic<std::uint32_t>> ssrc_;
            uint32_t ts_;
            uint16_t seq_;
            rtp_format_t fmt_;
            uint8_t payload_;

            uint32_t clock_rate_;
            std::chrono::time_point<std::chrono::high_resolution_clock> wc_start_;
            uvgrtp::clock::hrc::hrc_t wc_start_2;

            size_t sent_pkts_;

            /* Use custom timestamp for the outgoing RTP packets */
            uint64_t timestamp_;

            /* custom NTP timestamp of when the RTP packet was SAMPLED */
            uint64_t sampling_ntp_;

            /* Last RTP timestamp. The 2 timestamps above are initial timestamps, this is the 
             * one that gets updated */
            uint32_t rtp_ts_;

            /* What is the maximum size of the payload available for this RTP instance */
            size_t payload_size_;

            /* What is the maximum delay allowed for each frame
             * i.e. how long does the packet receiver wait for
             * all fragments of a frame until it's considered late and dropped
             *
             * Default value is 100ms */
            size_t delay_;
    };
}

namespace uvg_rtp = uvgrtp;
