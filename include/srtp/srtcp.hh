#pragma once

#include "base.hh"

namespace uvg_rtp {

    class srtcp : public base_srtp {
        public:
            srtcp();
            ~srtcp();

            /* Encrypt the RTCP packet
             *
             * Report RTP_OK on succes
             * Return RTP_INVALID_VALUE if IV creation fails */
            rtp_error_t encrypt(uint32_t ssrc, uint16_t seq, uint8_t *buffer, size_t len);

            /* Calculate authentication tag for the RTCP packet
             *
             * Return RTP_OK on success
             * Return RTP_INVALID_VALUE if "buffer" or "len" is invalid */
            rtp_error_t add_auth_tag(uint8_t *buffer, size_t len);

            /* Verify the authentication tag present in "buffer"
             *
             * Return RTP_OK on success
             * Return RTP_AUTH_TAG_MISMATCH if authentication tags don't match */
            rtp_error_t verify_auth_tag(uint8_t *buffer, size_t len);

            /* Decrypt and verify the authenticity of the RTCP packet
             *
             * Report RTP_OK on success
             * Return RTP_INVALID_VALUE if IV creation fails
             * Return RTP_AUTH_TAG_MISMATCH if authentication tag is incorrect
             * Return RTP_MEMORY_ERROR if memory allocation fails */
            rtp_error_t decrypt(uint32_t ssrc, uint32_t seq, uint8_t *buffer, size_t len);
    };
};
