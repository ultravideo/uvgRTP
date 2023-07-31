#pragma once

#include "base.hh"

namespace uvgrtp {

    namespace frame {
        struct rtp_frame;
    }

    class srtp : public base_srtp {
        public:
            srtp(int rce_flags);
            ~srtp();

            /* Decrypt the payload of an RTP packet and verify authentication tag (if enabled) */
            rtp_error_t recv_packet_handler(void* args, int rce_flags, uint8_t* read_ptr, size_t size, uvgrtp::frame::rtp_frame** out);

            /* Encrypt the payload of an RTP packet and add authentication tag (if enabled) */
            static rtp_error_t send_packet_handler(void *arg, buf_vec& buffers);

        private:
            /* TODO:  */
            rtp_error_t encrypt(uint32_t ssrc, uint16_t seq, uint8_t* buffer, size_t len);

            /* Has RTP packet authentication been enabled? */
            bool authenticate_rtp() const;

            /* By default RTP packet authentication is disabled but by
             * giving RCE_SRTP_AUTHENTICATE_RTP to create_stream() user can enable it.
             *
             * The authentication tag will occupy the last 8 bytes of the RTP packet */
            bool authenticate_rtp_;

    };
}

namespace uvg_rtp = uvgrtp;
