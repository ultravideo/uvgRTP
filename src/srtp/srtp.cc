#include "srtp.hh"

#include "base.hh"
#include "crypto.hh"
#include "debug.hh"
#include "frame.hh"

#include <cstring>
#include <iostream>


#define MAX_OFF 10000

uvgrtp::srtp::srtp(int flags):base_srtp(),
      authenticate_rtp_(flags & RCE_SRTP_AUTHENTICATE_RTP)
{}

uvgrtp::srtp::~srtp()
{}

rtp_error_t uvgrtp::srtp::encrypt(uint32_t ssrc, uint16_t seq, uint8_t *buffer, size_t len)
{
    if (use_null_cipher_)
        return RTP_OK;

    uint8_t iv[UVG_IV_LENGTH] = { 0 };
    uint64_t index = (((uint64_t)srtp_ctx_->roc) << 16) + seq;

    /* Sequence number has wrapped around, update Roll-over Counter */
    if (seq == 0xffff)
        srtp_ctx_->roc++;

    if (create_iv(iv, ssrc, index, srtp_ctx_->key_ctx.local.salt_key) != RTP_OK) {
        LOG_ERROR("Failed to create IV, unable to encrypt the RTP packet!");
        return RTP_INVALID_VALUE;
    }

    uvgrtp::crypto::aes::ctr ctr(srtp_ctx_->key_ctx.local.enc_key, srtp_ctx_->n_e, iv);
    ctr.encrypt(buffer, buffer, len);

    return RTP_OK;
}

rtp_error_t uvgrtp::srtp::recv_packet_handler(void *arg, int flags, frame::rtp_frame **out)
{
    (void)flags;

    auto srtp  = (uvgrtp::srtp *)arg;
    auto ctx   = srtp->get_ctx();
    auto frame = *out;

    /* Calculate authentication tag for the packet and compare it against the one we received */
    if (srtp->authenticate_rtp()) {
        uint8_t digest[10] = { 0 };
        auto hmac_sha1     = uvgrtp::crypto::hmac::sha1(ctx->key_ctx.remote.auth_key, UVG_AUTH_LENGTH);

        hmac_sha1.update(frame->dgram, frame->dgram_size - UVG_AUTH_TAG_LENGTH);
        hmac_sha1.update((uint8_t *)&ctx->roc, sizeof(ctx->roc));
        hmac_sha1.final((uint8_t *)digest, UVG_AUTH_TAG_LENGTH);

        if (memcmp(digest, &frame->dgram[frame->dgram_size - UVG_AUTH_TAG_LENGTH], UVG_AUTH_TAG_LENGTH)) {
            LOG_ERROR("Authentication tag mismatch!");
            return RTP_GENERIC_ERROR;
        }

        if (srtp->is_replayed_packet(digest)) {
            LOG_ERROR("Replayed packet received, discarding!");
            return RTP_GENERIC_ERROR;
        }
        frame->payload_len -= UVG_AUTH_TAG_LENGTH;
    }

    if (srtp->use_null_cipher())
        return RTP_PKT_NOT_HANDLED;

    uint8_t iv[UVG_IV_LENGTH] = { 0 };
    uint16_t seq          = frame->header.seq;
    uint32_t ssrc         = frame->header.ssrc;
    uint32_t ts           = frame->header.timestamp;
    uint64_t index        = 0;

    /* as the sequence number approaches 0xffff and is close to wrapping around,
     * special care must be taken to use correct roll-over counter as it's entirely
     * possible that packets come out of order around this overflow boundary
     * and if e.g. we first receive packet with sequence number 0xffff and thus update
     * ROC to ROC + 1 and after that we receive packet with sequence number 0xfffe,
     * we use an incorrect value for ROC as the the packet 0xfffe was encrypted with ROC - 1.
     *
     * It is a reasonable assumption that correct ROC differs from "ctx->roc" at most by 1 (-, +)
     * because if the difference is more than 1, the input frame would be larger than 90 MB.
     *
     * Here the assumption is that the offset for an incorrectly ordered packet is at most 10k */
    if (ts == ctx->rts && (uint16_t)(seq + MAX_OFF) < MAX_OFF)
        index = (((uint64_t)ctx->roc - 1) << 16) + seq;
    else
        index = (((uint64_t)ctx->roc) << 16) + seq;

    /* Sequence number has wrapped around, update Roll-over Counter */
    if (seq == 0xffff) {
        ctx->roc++;
        ctx->rts = ts;
    }

    if (srtp->create_iv(iv, ssrc, index, ctx->key_ctx.remote.salt_key) != RTP_OK) {
        LOG_ERROR("Failed to create IV, unable to encrypt the RTP packet!");
        return RTP_GENERIC_ERROR;
    }

    uvgrtp::crypto::aes::ctr ctr(ctx->key_ctx.remote.enc_key, ctx->n_e, iv);
    ctr.decrypt(frame->payload, frame->payload, frame->payload_len);

    return RTP_PKT_MODIFIED;
}

rtp_error_t uvgrtp::srtp::send_packet_handler(void *arg, uvgrtp::buf_vec& buffers)
{
    auto srtp       = (uvgrtp::srtp *)arg;
    auto frame      = (uvgrtp::frame::rtp_frame *)buffers.at(0).second;
    auto ctx        = srtp->get_ctx();
    auto off        = srtp->authenticate_rtp() ? 2 : 1;
    auto data       = buffers.at(buffers.size() - off);
    auto hmac_sha1  = uvgrtp::crypto::hmac::sha1(ctx->key_ctx.local.auth_key, UVG_AUTH_LENGTH);
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
    hmac_sha1.final((uint8_t *)buffers[buffers.size() - 1].second, UVG_AUTH_TAG_LENGTH);

    return ret;
}

bool uvgrtp::srtp::authenticate_rtp()
{
    return authenticate_rtp_;
}

