#include <cstring>
#include <iostream>

#include "srtp/base.hh"
#include "srtp/srtp.hh"

#ifdef __RTP_CRYPTO__
#include "crypto.hh"
#include <cryptopp/hex.h>
#endif

uvg_rtp::srtp::srtp()
{
}

uvg_rtp::srtp::~srtp()
{
}

#ifdef __RTP_CRYPTO__
rtp_error_t uvg_rtp::srtp::encrypt(uint32_t ssrc, uint16_t seq, uint8_t *buffer, size_t len)
{
    if (use_null_cipher_)
        return RTP_OK;

    uint8_t iv[16] = { 0 };
    uint64_t index = (((uint64_t)srtp_ctx_->roc) << 16) + seq;

    /* Sequence number has wrapped around, update Roll-over Counter */
    if (seq == 0xffff)
        srtp_ctx_->roc++;

    if (create_iv(iv, ssrc, index, srtp_ctx_->key_ctx.local.salt_key) != RTP_OK) {
        LOG_ERROR("Failed to create IV, unable to encrypt the RTP packet!");
        return RTP_INVALID_VALUE;
    }

    uvg_rtp::crypto::aes::ctr ctr(srtp_ctx_->key_ctx.local.enc_key, sizeof(srtp_ctx_->key_ctx.local.enc_key), iv);
    ctr.encrypt(buffer, buffer, len);

    return RTP_OK;
}

rtp_error_t uvg_rtp::srtp::recv_packet_handler(void *arg, int flags, frame::rtp_frame **out)
{
    (void)flags;

    auto srtp  = (uvg_rtp::srtp *)arg;
    auto ctx   = srtp->get_ctx();
    auto frame = *out;

    /* Calculate authentication tag for the packet and compare it against the one we received */
    if (srtp->authenticate_rtp()) {
        uint32_t digest = 0;
        auto hmac_sha1  = uvg_rtp::crypto::hmac::sha1(ctx->key_ctx.remote.auth_key, AES_KEY_LENGTH);

        hmac_sha1.update(frame->dgram, frame->dgram_size - AUTH_TAG_LENGTH);
        hmac_sha1.update((uint8_t *)&ctx->roc, sizeof(ctx->roc));
        hmac_sha1.final((uint8_t *)&digest, sizeof(uint32_t));

        if (memcmp(&digest, &frame->dgram[frame->dgram_size - AUTH_TAG_LENGTH], AUTH_TAG_LENGTH)) {
            LOG_ERROR("Authentication tag mismatch!");
            return RTP_GENERIC_ERROR;
        }

        frame->payload_len -= AUTH_TAG_LENGTH;
    }

    if (srtp->use_null_cipher())
        return RTP_PKT_NOT_HANDLED;

    uint8_t iv[16]  = { 0 };
    uint16_t seq    = frame->header.seq;
    uint32_t ssrc   = frame->header.ssrc;
    uint64_t index  = (((uint64_t)ctx->roc) << 16) + seq;

    /* Sequence number has wrapped around, update Roll-over Counter */
    if (seq == 0xffff)
        ctx->roc++;

    if (srtp->create_iv(iv, ssrc, index, ctx->key_ctx.remote.salt_key) != RTP_OK) {
        LOG_ERROR("Failed to create IV, unable to encrypt the RTP packet!");
        return RTP_GENERIC_ERROR;
    }

    uvg_rtp::crypto::aes::ctr ctr(ctx->key_ctx.remote.enc_key, sizeof(ctx->key_ctx.remote.enc_key), iv);
    ctr.decrypt(frame->payload, frame->payload, frame->payload_len);

    return RTP_PKT_MODIFIED;
}

rtp_error_t uvg_rtp::srtp::send_packet_handler(void *arg, uvg_rtp::buf_vec& buffers)
{
    auto srtp       = (uvg_rtp::srtp *)arg;
    auto frame      = (uvg_rtp::frame::rtp_frame *)buffers.at(0).second;
    auto ctx        = srtp->get_ctx();
    auto off        = srtp->authenticate_rtp() ? 2 : 1;
    auto data       = buffers.at(buffers.size() - off);
    auto hmac_sha1  = uvg_rtp::crypto::hmac::sha1(ctx->key_ctx.local.auth_key, AES_KEY_LENGTH);
    rtp_error_t ret = RTP_OK;

    if (srtp->use_null_cipher())
        goto authenticate;

    ret = srtp->encrypt(
        ntohl(frame->header.ssrc),
        ntohs(frame->header.seq),
        data.second,
        data.first
    );

    if (ret != RTP_OK) {
        LOG_ERROR("Failed to encrypt RTP packet!");
        return ret;
    }

authenticate:
    if (!srtp->authenticate_rtp())
        return RTP_OK;

    for (size_t i = 0; i < buffers.size() - 1; ++i)
        hmac_sha1.update((uint8_t *)buffers[i].second, buffers[i].first);

    hmac_sha1.update((uint8_t *)&ctx->roc, sizeof(ctx->roc));
    hmac_sha1.final((uint8_t *)buffers[buffers.size() - 1].second, sizeof(uint32_t));

    return ret;
}
#endif
