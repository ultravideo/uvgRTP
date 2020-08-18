#include <cstring>
#include <iostream>

#include "srtp.hh"

#ifdef __RTP_CRYPTO__
#include "crypto.hh"
#include <cryptopp/hex.h>
#endif

uvg_rtp::srtp::srtp():
    srtp_ctx_(),
    use_null_cipher_(false),
    authenticate_rtp_(false)
{
}

uvg_rtp::srtp::~srtp()
{
}

#ifdef __RTP_CRYPTO__
rtp_error_t uvg_rtp::srtp::derive_key(int label, uint8_t *key, uint8_t *salt, uint8_t *out, size_t out_len)
{
    uint8_t input[AES_KEY_LENGTH] = { 0 };
    memcpy(input, salt, SALT_LENGTH);

    input[7] ^= label;
    memset(out, 0, out_len);

    uvg_rtp::crypto::aes::ecb ecb(key, AES_KEY_LENGTH);
    ecb.encrypt(out, input, AES_KEY_LENGTH);

    return RTP_OK;
}

rtp_error_t uvg_rtp::srtp::create_iv(uint8_t *out, uint32_t ssrc, uint64_t index, uint8_t *salt)
{
    if (!out || !salt)
        return RTP_INVALID_VALUE;

    uint8_t buf[8];
    int i;

    memset(out,         0,  AES_KEY_LENGTH);
    memcpy(&out[4], &ssrc,  sizeof(uint32_t));
    memcpy(buf,     &index, sizeof(uint64_t));

    for (i = 0; i < 8; i++)
        out[6 + i] ^= buf[i];

    for (i = 0; i < 14; i++)
        out[i] ^= salt[i];

    return RTP_OK;
}

rtp_error_t uvg_rtp::srtp::encrypt(uint32_t ssrc, uint16_t seq, uint8_t *buffer, size_t len)
{
    if (use_null_cipher_)
        return RTP_OK;

    uint8_t iv[16] = { 0 };
    uint64_t index = (((uint64_t)srtp_ctx_.roc) << 16) + seq;

    /* Sequence number has wrapped around, update Roll-over Counter */
    if (seq == 0xffff)
        srtp_ctx_.roc++;

    if (create_iv(iv, ssrc, index, srtp_ctx_.key_ctx.local.salt_key) != RTP_OK) {
        LOG_ERROR("Failed to create IV, unable to encrypt the RTP packet!");
        return RTP_INVALID_VALUE;
    }

    uvg_rtp::crypto::aes::ctr ctr(srtp_ctx_.key_ctx.local.enc_key, sizeof(srtp_ctx_.key_ctx.local.enc_key), iv);
    ctr.encrypt(buffer, buffer, len);

    return RTP_OK;
}

bool uvg_rtp::srtp::use_null_cipher()
{
    return use_null_cipher_;
}

bool uvg_rtp::srtp::authenticate_rtp()
{
    return authenticate_rtp_;
}

uvg_rtp::srtp_ctx_t& uvg_rtp::srtp::get_ctx()
{
    return srtp_ctx_;
}

rtp_error_t uvg_rtp::srtp::__init(int type, int flags)
{
    srtp_ctx_.roc  = 0;
    srtp_ctx_.type = type;
    srtp_ctx_.enc  = AES_128;
    srtp_ctx_.hmac = HMAC_SHA1;

    srtp_ctx_.mki_size    = 0;
    srtp_ctx_.mki_present = false;
    srtp_ctx_.mki         = nullptr;

    srtp_ctx_.master_key  = srtp_ctx_.key_ctx.master.local_key;
    srtp_ctx_.master_salt = srtp_ctx_.key_ctx.master.local_salt;
    srtp_ctx_.mk_cnt      = 0;

    srtp_ctx_.n_e = AES_KEY_LENGTH;
    srtp_ctx_.n_a = HMAC_KEY_LENGTH;

    srtp_ctx_.s_l    = 0;
    srtp_ctx_.replay = nullptr;

    use_null_cipher_  = !!(flags & RCE_SRTP_NULL_CIPHER);
    authenticate_rtp_ = !!(flags & RCE_SRTP_AUTHENTICATE_RTP);

    /* Local aka encryption keys */
    (void)derive_key(
        SRTP_ENCRYPTION,
        srtp_ctx_.key_ctx.master.local_key,
        srtp_ctx_.key_ctx.master.local_salt,
        srtp_ctx_.key_ctx.local.enc_key,
        AES_KEY_LENGTH
    );
    (void)derive_key(
        SRTP_AUTHENTICATION,
        srtp_ctx_.key_ctx.master.local_key,
        srtp_ctx_.key_ctx.master.local_salt,
        srtp_ctx_.key_ctx.local.auth_key,
        AES_KEY_LENGTH
    );
    (void)derive_key(
        SRTP_SALTING,
        srtp_ctx_.key_ctx.master.local_key,
        srtp_ctx_.key_ctx.master.local_salt,
        srtp_ctx_.key_ctx.local.salt_key,
        SALT_LENGTH
    );

    /* Remote aka decryption keys */
    (void)derive_key(
        SRTP_ENCRYPTION,
        srtp_ctx_.key_ctx.master.remote_key,
        srtp_ctx_.key_ctx.master.remote_salt,
        srtp_ctx_.key_ctx.remote.enc_key,
        AES_KEY_LENGTH
    );
    (void)derive_key(
        SRTP_AUTHENTICATION,
        srtp_ctx_.key_ctx.master.remote_key,
        srtp_ctx_.key_ctx.master.remote_salt,
        srtp_ctx_.key_ctx.remote.auth_key,
        AES_KEY_LENGTH
    );
    (void)derive_key(
        SRTP_SALTING,
        srtp_ctx_.key_ctx.master.remote_key,
        srtp_ctx_.key_ctx.master.remote_salt,
        srtp_ctx_.key_ctx.remote.salt_key,
        SALT_LENGTH
    );

    return RTP_OK;
}

rtp_error_t uvg_rtp::srtp::init_zrtp(int type, int flags, uvg_rtp::rtp *rtp, uvg_rtp::zrtp *zrtp)
{
    (void)rtp;

    if (!zrtp)
        return RTP_INVALID_VALUE;

    if (type != SRTP) {
        LOG_ERROR("SRTCP not supported!");
        return RTP_INVALID_VALUE;
    }

    /* ZRTP key derivation function expects the keys lengths to be given in bits */
    rtp_error_t ret = zrtp->get_srtp_keys(
        srtp_ctx_.key_ctx.master.local_key,   AES_KEY_LENGTH * 8,
        srtp_ctx_.key_ctx.master.remote_key,  AES_KEY_LENGTH * 8,
        srtp_ctx_.key_ctx.master.local_salt,  SALT_LENGTH    * 8,
        srtp_ctx_.key_ctx.master.remote_salt, SALT_LENGTH    * 8
    );

    if (ret != RTP_OK) {
        LOG_ERROR("Failed to derive keys for SRTP session!");
        return ret;
    }

    return __init(type, flags);
}

rtp_error_t uvg_rtp::srtp::init_user(int type, int flags, uint8_t *key, uint8_t *salt)
{
    if (!key || !salt)
        return RTP_INVALID_VALUE;

    memcpy(srtp_ctx_.key_ctx.master.local_key,    key, AES_KEY_LENGTH);
    memcpy(srtp_ctx_.key_ctx.master.remote_key,   key, AES_KEY_LENGTH);
    memcpy(srtp_ctx_.key_ctx.master.local_salt,  salt, SALT_LENGTH);
    memcpy(srtp_ctx_.key_ctx.master.remote_salt, salt, SALT_LENGTH);

    return __init(type, flags);
}

rtp_error_t uvg_rtp::srtp::recv_packet_handler(void *arg, int flags, frame::rtp_frame **out)
{
    (void)flags;

    uvg_rtp::srtp *srtp              = (uvg_rtp::srtp *)arg;
    uvg_rtp::srtp_ctx_t ctx          = srtp->get_ctx();
    uvg_rtp::frame::rtp_frame *frame = *out;

    if (srtp->use_null_cipher())
        return RTP_PKT_NOT_HANDLED;

    uint8_t iv[16]  = { 0 };
    uint16_t seq    = frame->header.seq;
    uint32_t ssrc   = frame->header.ssrc;
    uint64_t index  = (((uint64_t)ctx.roc) << 16) + seq;
    uint64_t digest = 0;

    /* Sequence number has wrapped around, update Roll-over Counter */
    if (seq == 0xffff)
        ctx.roc++;

    if (srtp->create_iv(iv, ssrc, index, ctx.key_ctx.remote.salt_key) != RTP_OK) {
        LOG_ERROR("Failed to create IV, unable to encrypt the RTP packet!");
        return RTP_INVALID_VALUE;
    }

    uvg_rtp::crypto::aes::ctr ctr(ctx.key_ctx.remote.enc_key, sizeof(ctx.key_ctx.remote.enc_key), iv);

    /* exit early if RTP packet authentication is disabled... */
    if (!srtp->authenticate_rtp()) {
        ctr.decrypt(frame->payload, frame->payload, frame->payload_len);
        return RTP_OK;
    }

    /* ... otherwise calculate authentication tag for the packet
     * and compare it against the one we received */
    auto hmac_sha1 = uvg_rtp::crypto::hmac::sha1(ctx.key_ctx.remote.auth_key, AES_KEY_LENGTH);

    hmac_sha1.update((uint8_t *)frame, frame->payload_len - AUTH_TAG_LENGTH);
    hmac_sha1.update((uint8_t *)&ctx.roc, sizeof(ctx.roc));
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
    auto hmac_sha1 = uvg_rtp::crypto::hmac::sha1(ctx.key_ctx.local.auth_key, AES_KEY_LENGTH);

    for (size_t i = 0; i < buffers.size() - 1; ++i)
        hmac_sha1.update((uint8_t *)buffers[i].second, buffers[i].first);

    hmac_sha1.update((uint8_t *)&ctx.roc, sizeof(ctx.roc));
    hmac_sha1.final((uint8_t *)buffers[buffers.size() - 1].second);

    return ret;
}
#endif
