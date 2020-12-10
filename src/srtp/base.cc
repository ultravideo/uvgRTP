#include <cstring>
#include <iostream>

#include "crypto.hh"
#include "srtp/base.hh"

uvg_rtp::base_srtp::base_srtp():
    srtp_ctx_(new uvg_rtp::srtp_ctx_t),
    use_null_cipher_(false),
    authenticate_rtp_(false)
{
}

uvg_rtp::base_srtp::~base_srtp()
{
}

bool uvg_rtp::base_srtp::use_null_cipher()
{
    return use_null_cipher_;
}

bool uvg_rtp::base_srtp::authenticate_rtp()
{
    return authenticate_rtp_;
}

uvg_rtp::srtp_ctx_t *uvg_rtp::base_srtp::get_ctx()
{
    return srtp_ctx_;
}

rtp_error_t uvg_rtp::base_srtp::derive_key(int label, uint8_t *key, uint8_t *salt, uint8_t *out, size_t out_len)
{
    uint8_t input[AES_KEY_LENGTH] = { 0 };
    memcpy(input, salt, SALT_LENGTH);

    input[7] ^= label;
    memset(out, 0, out_len);

    uvg_rtp::crypto::aes::ecb ecb(key, AES_KEY_LENGTH);
    ecb.encrypt(out, input, AES_KEY_LENGTH);

    return RTP_OK;
}

rtp_error_t uvg_rtp::base_srtp::create_iv(uint8_t *out, uint32_t ssrc, uint64_t index, uint8_t *salt)
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

bool uvg_rtp::base_srtp::is_replayed_packet(uint8_t *digest)
{
    if (!(srtp_ctx_->flags & RCE_SRTP_REPLAY_PROTECTION))
        return false;

    uint64_t truncated;
    memcpy(&truncated, digest, sizeof(uint64_t));

    if (replay_list_.find(truncated) != replay_list_.end()) {
        LOG_ERROR("Replayed packet received, discarding!");
        return true;
    }

    replay_list_.insert(truncated);
    return false;
}

rtp_error_t uvg_rtp::base_srtp::init(int type, int flags)
{
    srtp_ctx_->roc  = 0;
    srtp_ctx_->rts  = 0;
    srtp_ctx_->type = type;
    srtp_ctx_->enc  = AES_128;
    srtp_ctx_->hmac = HMAC_SHA1;

    srtp_ctx_->mki_size    = 0;
    srtp_ctx_->mki_present = false;
    srtp_ctx_->mki         = nullptr;

    srtp_ctx_->master_key  = srtp_ctx_->key_ctx.master.local_key;
    srtp_ctx_->master_salt = srtp_ctx_->key_ctx.master.local_salt;
    srtp_ctx_->mk_cnt      = 0;

    srtp_ctx_->n_e = AES_KEY_LENGTH;
    srtp_ctx_->n_a = HMAC_KEY_LENGTH;

    srtp_ctx_->s_l    = 0;
    srtp_ctx_->replay = nullptr;

    use_null_cipher_  = !!(flags & RCE_SRTP_NULL_CIPHER);
    authenticate_rtp_ = !!(flags & RCE_SRTP_AUTHENTICATE_RTP);

    srtp_ctx_->flags  = flags;

    int label_enc  = 0;
    int label_auth = 0;
    int label_salt = 0;

    if (type == SRTP) {
        label_enc  = SRTP_ENCRYPTION;
        label_auth = SRTP_AUTHENTICATION;
        label_salt = SRTP_SALTING;
    } else {
        label_enc  = SRTCP_ENCRYPTION;
        label_auth = SRTCP_AUTHENTICATION;
        label_salt = SRTCP_SALTING;
    }

    /* Local aka encryption keys */
    (void)derive_key(
        label_enc,
        srtp_ctx_->key_ctx.master.local_key,
        srtp_ctx_->key_ctx.master.local_salt,
        srtp_ctx_->key_ctx.local.enc_key,
        AES_KEY_LENGTH
    );
    (void)derive_key(
        label_auth,
        srtp_ctx_->key_ctx.master.local_key,
        srtp_ctx_->key_ctx.master.local_salt,
        srtp_ctx_->key_ctx.local.auth_key,
        AES_KEY_LENGTH
    );
    (void)derive_key(
        label_salt,
        srtp_ctx_->key_ctx.master.local_key,
        srtp_ctx_->key_ctx.master.local_salt,
        srtp_ctx_->key_ctx.local.salt_key,
        SALT_LENGTH
    );

    /* Remote aka decryption keys */
    (void)derive_key(
        label_enc,
        srtp_ctx_->key_ctx.master.remote_key,
        srtp_ctx_->key_ctx.master.remote_salt,
        srtp_ctx_->key_ctx.remote.enc_key,
        AES_KEY_LENGTH
    );
    (void)derive_key(
        label_auth,
        srtp_ctx_->key_ctx.master.remote_key,
        srtp_ctx_->key_ctx.master.remote_salt,
        srtp_ctx_->key_ctx.remote.auth_key,
        AES_KEY_LENGTH
    );
    (void)derive_key(
        label_salt,
        srtp_ctx_->key_ctx.master.remote_key,
        srtp_ctx_->key_ctx.master.remote_salt,
        srtp_ctx_->key_ctx.remote.salt_key,
        SALT_LENGTH
    );

    return RTP_OK;
}

rtp_error_t uvg_rtp::base_srtp::init_zrtp(int type, int flags, uvg_rtp::rtp *rtp, uvg_rtp::zrtp *zrtp)
{
    (void)rtp;

    if (!zrtp)
        return RTP_INVALID_VALUE;

    /* ZRTP key derivation function expects the keys lengths to be given in bits */
    rtp_error_t ret = zrtp->get_srtp_keys(
        srtp_ctx_->key_ctx.master.local_key,   AES_KEY_LENGTH * 8,
        srtp_ctx_->key_ctx.master.remote_key,  AES_KEY_LENGTH * 8,
        srtp_ctx_->key_ctx.master.local_salt,  SALT_LENGTH    * 8,
        srtp_ctx_->key_ctx.master.remote_salt, SALT_LENGTH    * 8
    );

    if (ret != RTP_OK) {
        LOG_ERROR("Failed to derive keys for SRTP session!");
        return ret;
    }

    return init(type, flags);
}

rtp_error_t uvg_rtp::base_srtp::init_user(int type, int flags, uint8_t *key, uint8_t *salt)
{
    if (!key || !salt)
        return RTP_INVALID_VALUE;

    memcpy(srtp_ctx_->key_ctx.master.local_key,    key, AES_KEY_LENGTH);
    memcpy(srtp_ctx_->key_ctx.master.remote_key,   key, AES_KEY_LENGTH);
    memcpy(srtp_ctx_->key_ctx.master.local_salt,  salt, SALT_LENGTH);
    memcpy(srtp_ctx_->key_ctx.master.remote_salt, salt, SALT_LENGTH);

    return init(type, flags);
}
