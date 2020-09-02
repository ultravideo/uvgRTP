#include <cstring>
#include <iostream>

#include "srtp/srtcp.hh"

#ifdef __RTP_CRYPTO__
#include "crypto.hh"
#include <cryptopp/hex.h>
#endif

uvg_rtp::srtcp::srtcp()
{
}

uvg_rtp::srtcp::~srtcp()
{
}

#ifdef __RTP_CRYPTO__
rtp_error_t uvg_rtp::srtcp::encrypt(uint32_t ssrc, uint16_t seq, uint8_t *buffer, size_t len)
{
    if (use_null_cipher_)
        return RTP_OK;

    uint8_t iv[16] = { 0 };

    if (create_iv(iv, ssrc, seq, srtp_ctx_->key_ctx.local.salt_key) != RTP_OK) {
        LOG_ERROR("Failed to create IV, unable to encrypt the RTP packet!");
        return RTP_INVALID_VALUE;
    }

    uvg_rtp::crypto::aes::ctr ctr(srtp_ctx_->key_ctx.local.enc_key, sizeof(srtp_ctx_->key_ctx.local.enc_key), iv);
    ctr.encrypt(buffer, buffer, len);

    return RTP_OK;
}

rtp_error_t uvg_rtp::srtcp::decrypt(uint32_t ssrc, uint32_t seq, uint8_t *buffer, size_t size)
{
    uint8_t iv[16]  = { 0 };
    uint64_t digest = 0;

    if (create_iv(iv, ssrc, seq, srtp_ctx_->key_ctx.remote.salt_key) != RTP_OK) {
        LOG_ERROR("Failed to create IV, unable to encrypt the RTP packet!");
        return RTP_INVALID_VALUE;
    }

    uvg_rtp::crypto::aes::ctr ctr(srtp_ctx_->key_ctx.remote.enc_key, sizeof(srtp_ctx_->key_ctx.remote.enc_key), iv);

    /* ... otherwise calculate authentication tag for the packet
     * and compare it against the one we received */
    auto hmac_sha1 = uvg_rtp::crypto::hmac::sha1(srtp_ctx_->key_ctx.remote.auth_key, AES_KEY_LENGTH);

    hmac_sha1.update(buffer, size - AUTH_TAG_LENGTH - SRTCP_INDEX_LENGTH);
    hmac_sha1.final((uint8_t *)&digest);

    if (memcmp(&digest, &buffer[size - AUTH_TAG_LENGTH - SRTCP_INDEX_LENGTH], AUTH_TAG_LENGTH)) {
        LOG_ERROR("Authentication tag mismatch!");
        return RTP_AUTH_TAG_MISMATCH;
    }

    size_t size_ = size - AUTH_TAG_LENGTH - SRTCP_INDEX_LENGTH - RTCP_HEADER_LENGTH;
    uint8_t *new_buffer = new uint8_t[size_];

    ctr.decrypt(new_buffer, buffer + RTCP_HEADER_LENGTH, size_);
    memset(buffer + RTCP_HEADER_LENGTH, 0, size);
    memcpy(buffer + RTCP_HEADER_LENGTH, new_buffer, size_);
    delete[] new_buffer;

    return RTP_OK;
}
#endif
