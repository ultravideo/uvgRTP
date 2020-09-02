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

    uvg_rtp::srtp *srtp              = (uvg_rtp::srtp *)arg;
    uvg_rtp::srtp_ctx_t *ctx         = srtp->get_ctx();
    uvg_rtp::frame::rtp_frame *frame = *out;

    if (srtp->use_null_cipher())
        return RTP_PKT_NOT_HANDLED;

    uint8_t iv[16]  = { 0 };
    uint16_t seq    = frame->header.seq;
    uint32_t ssrc   = frame->header.ssrc;
    uint64_t index  = (((uint64_t)ctx->roc) << 16) + seq;
    uint64_t digest = 0;

    /* Sequence number has wrapped around, update Roll-over Counter */
    if (seq == 0xffff)
        ctx->roc++;

    if (srtp->create_iv(iv, ssrc, index, ctx->key_ctx.remote.salt_key) != RTP_OK) {
        LOG_ERROR("Failed to create IV, unable to encrypt the RTP packet!");
        return RTP_INVALID_VALUE;
    }

    uvg_rtp::crypto::aes::ctr ctr(ctx->key_ctx.remote.enc_key, sizeof(ctx->key_ctx.remote.enc_key), iv);

    /* exit early if RTP packet authentication is disabled... */
    if (!srtp->authenticate_rtp()) {
        ctr.decrypt(frame->payload, frame->payload, frame->payload_len);
        return RTP_OK;
    }

    /* ... otherwise calculate authentication tag for the packet
     * and compare it against the one we received */
    auto hmac_sha1 = uvg_rtp::crypto::hmac::sha1(ctx->key_ctx.remote.auth_key, AES_KEY_LENGTH);

    hmac_sha1.update((uint8_t *)frame, frame->payload_len - AUTH_TAG_LENGTH);
    hmac_sha1.update((uint8_t *)&ctx->roc, sizeof(ctx->roc));
    hmac_sha1.final((uint8_t *)&digest);

    if (memcmp(&digest, &frame[frame->payload_len - AUTH_TAG_LENGTH], AUTH_TAG_LENGTH)) {
        LOG_ERROR("Authentication tag mismatch!");
        return RTP_AUTH_TAG_MISMATCH;
    }

    size_t size_ = frame->payload_len - sizeof(uvg_rtp::frame::rtp_header) - 8;
    uint8_t *new_buffer = new uint8_t[size_];

    ctr.decrypt(new_buffer, frame->payload, size_);
    memset(frame->payload, 0, frame->payload_len);
    memcpy(frame->payload, new_buffer, size_);
    delete[] new_buffer;

    return RTP_OK;
}

rtp_error_t uvg_rtp::srtp::send_packet_handler(void *arg, uvg_rtp::buf_vec& buffers)
{
    auto srtp = (uvg_rtp::srtp *)arg;

    if (srtp->use_null_cipher())
        return RTP_OK;

    auto frame  = (uvg_rtp::frame::rtp_frame *)buffers.at(0).second;
    auto rtp    = buffers.at(buffers.size() - 1);
    auto ctx    = srtp->get_ctx();

    rtp_error_t ret = srtp->encrypt(
        ntohl(frame->header.ssrc),
        ntohs(frame->header.seq),
        rtp.second,
        rtp.first
    );

    if (!srtp->authenticate_rtp())
        return RTP_OK;

    /* create authentication tag for the packet and push it to the vector buffer */
    auto hmac_sha1 = uvg_rtp::crypto::hmac::sha1(ctx->key_ctx.local.auth_key, AES_KEY_LENGTH);

    for (size_t i = 0; i < buffers.size() - 1; ++i)
        hmac_sha1.update((uint8_t *)buffers[i].second, buffers[i].first);

    hmac_sha1.update((uint8_t *)&ctx->roc, sizeof(ctx->roc));
    hmac_sha1.final((uint8_t *)buffers[buffers.size() - 1].second);

    return ret;
}
#endif
