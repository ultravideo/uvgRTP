#pragma once

#include "base.hh"

namespace uvgrtp {

    namespace frame {
        struct rtp_frame;
    };

    class srtp : public base_srtp {
        public:
            srtp();
            ~srtp();

            /* TODO:  */
            rtp_error_t encrypt(uint32_t ssrc, uint16_t seq, uint8_t *buffer, size_t len);

            /* TODO:  */
            rtp_error_t decrypt(uint8_t *buffer, size_t len);

            /* Decrypt the payload of an RTP packet and verify authentication tag (if enabled) */
            static rtp_error_t recv_packet_handler(void *arg, int flags, frame::rtp_frame **out);

            /* Encrypt the payload of an RTP packet and add authentication tag (if enabled) */
            static rtp_error_t send_packet_handler(void *arg, buf_vec& buffers);
    };
};

namespace uvg_rtp = uvgrtp;
