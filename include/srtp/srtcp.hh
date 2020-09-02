#pragma once

#include "base.hh"

namespace uvg_rtp {

    class srtcp : public base_srtp {
        public:
            srtcp();
            ~srtcp();

            /* Encrypt the RTCP packet and calculate authentication tag for it
             *
             * Report RTP_OK on success
             * Return RTP_INVALID_VALUE if IV creation fails */
            rtp_error_t encrypt(uint32_t ssrc, uint16_t seq, uint8_t *buffer, size_t len);

            /* Decrypt and verify the authenticity of the RTCP packet
             *
             * Report RTP_OK on success
             * Return RTP_INVALID_VALUE if IV creation fails
             * Return RTP_AUTH_TAG_MISMATCH if authentication tag is incorrect
             * Return RTP_MEMORY_ERROR if memory allocation fails */
            rtp_error_t decrypt(uint32_t ssrc, uint32_t seq, uint8_t *buffer, size_t len);
    };
};
