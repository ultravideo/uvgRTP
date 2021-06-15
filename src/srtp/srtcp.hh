#pragma once

#include "base.hh"

namespace uvgrtp {

    class srtcp : public base_srtp {
        public:
            srtcp();
            ~srtcp();
            /* Encrypt and calculate authentication tag for the RTCP packet
             *
             * Report RTP_OK on succes
             * Return RTP_INVALID_VALUE if IV creation fails */
            rtp_error_t handle_rtcp_encryption(int flags, uint64_t packet_number,
                uint32_t ssrc, uint8_t* frame, size_t frame_size);

            /* Decrypt and verify the authenticity of the RTCP packet
             *
             * Report RTP_OK on success
             * Return RTP_INVALID_VALUE if IV creation fails
             * Return RTP_AUTH_TAG_MISMATCH if authentication tag is incorrect
             * Return RTP_MEMORY_ERROR if memory allocation fails */
            rtp_error_t handle_rtcp_decryption(int flags, uint32_t ssrc,
                uint8_t* packet, size_t packet_size);

    private:

        rtp_error_t encrypt(uint32_t ssrc, uint64_t seq, uint8_t* buffer, size_t len);
        rtp_error_t decrypt(uint32_t ssrc, uint32_t seq, uint8_t* buffer, size_t len);

        rtp_error_t add_auth_tag(uint8_t* buffer, size_t len);
        rtp_error_t verify_auth_tag(uint8_t* buffer, size_t len);
    };
};

namespace uvg_rtp = uvgrtp;
