#include "base.hh"

#include "crypto.hh"
#include "debug.hh"

#include <cstring>
#include <iostream>

uvgrtp::base_srtp::base_srtp():
    srtp_ctx_(new uvgrtp::srtp_ctx_t),
    use_null_cipher_(false)
{}

uvgrtp::base_srtp::~base_srtp()
{
    if (srtp_ctx_)
    {
        if (srtp_ctx_->key_ctx.master.local_key)
            delete[] srtp_ctx_->key_ctx.master.local_key;
        if (srtp_ctx_->key_ctx.master.remote_key)
            delete[] srtp_ctx_->key_ctx.master.remote_key;
        if (srtp_ctx_->key_ctx.local.enc_key)
            delete[] srtp_ctx_->key_ctx.local.enc_key;
        if (srtp_ctx_->key_ctx.remote.enc_key)
            delete[] srtp_ctx_->key_ctx.remote.enc_key;

        delete srtp_ctx_;
    }
}

bool uvgrtp::base_srtp::use_null_cipher()
{
    return use_null_cipher_;
}

uvgrtp::srtp_ctx_t *uvgrtp::base_srtp::get_ctx()
{
    return srtp_ctx_;
}

rtp_error_t uvgrtp::base_srtp::derive_key(int label, uint8_t *key, uint8_t *salt, uint8_t *out, size_t out_len)
{
    uint8_t input[UVG_IV_LENGTH]    = { 0 };
    uint8_t ks[AES128_KEY_SIZE] = { 0 };

    memcpy(input, salt, UVG_SALT_LENGTH);
    memset(out, 0, out_len);

    input[7] ^= label;

    uvgrtp::crypto::aes::ecb ecb(key, srtp_ctx_->n_e);
    ecb.encrypt(ks, input, UVG_IV_LENGTH);

    memcpy(out, ks, out_len);
    return RTP_OK;
}

rtp_error_t uvgrtp::base_srtp::create_iv(uint8_t *out, uint32_t ssrc, uint64_t index, uint8_t *salt)
{
    if (!out || !salt)
        return RTP_INVALID_VALUE;

    uint8_t buf[8];
    int i;

    memset(out,         0,  UVG_IV_LENGTH);
    memcpy(&out[4], &ssrc,  sizeof(uint32_t));
    memcpy(buf,     &index, sizeof(uint64_t));

    for (i = 0; i < 8; i++)
        out[6 + i] ^= buf[i];

    for (i = 0; i < 14; i++)
        out[i] ^= salt[i];

    return RTP_OK;
}

bool uvgrtp::base_srtp::is_replayed_packet(uint8_t *digest)
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

rtp_error_t uvgrtp::base_srtp::init(int type, int flags, uint8_t* local_key, uint8_t* remote_key,
                                    uint8_t* local_salt, uint8_t* remote_salt)
{
    srtp_ctx_->roc  = 0;
    srtp_ctx_->rts  = 0;
    srtp_ctx_->type = type;
    srtp_ctx_->hmac = HMAC_SHA1;

    size_t key_size = get_key_size(flags);

    rtp_error_t ret = RTP_OK;
    if ((ret = set_master_keys(key_size, local_key, remote_key, local_salt, remote_salt)) != RTP_OK)
        return ret;

    switch (key_size) {
        case AES128_KEY_SIZE:
            srtp_ctx_->enc  = AES_128;
            break;

        case AES192_KEY_SIZE:
            srtp_ctx_->enc  = AES_192;
            break;

        case AES256_KEY_SIZE:
            srtp_ctx_->enc  = AES_256;
            break;
    }

    srtp_ctx_->mki_size    = 0;
    srtp_ctx_->mki_present = false;
    srtp_ctx_->mki         = nullptr;

    srtp_ctx_->master_key  = srtp_ctx_->key_ctx.master.local_key;
    srtp_ctx_->master_salt = srtp_ctx_->key_ctx.master.local_salt;
    srtp_ctx_->mk_cnt      = 0;

    srtp_ctx_->n_e = key_size;
    srtp_ctx_->n_a = UVG_HMAC_KEY_LENGTH;

    srtp_ctx_->s_l    = 0;
    srtp_ctx_->replay = nullptr;

    use_null_cipher_  = (flags & RCE_SRTP_NULL_CIPHER);
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
        key_size
    );
    (void)derive_key(
        label_auth,
        srtp_ctx_->key_ctx.master.local_key,
        srtp_ctx_->key_ctx.master.local_salt,
        srtp_ctx_->key_ctx.local.auth_key,
        UVG_AUTH_LENGTH
    );
    (void)derive_key(
        label_salt,
        srtp_ctx_->key_ctx.master.local_key,
        srtp_ctx_->key_ctx.master.local_salt,
        srtp_ctx_->key_ctx.local.salt_key,
        UVG_SALT_LENGTH
    );

    /* Remote aka decryption keys */
    (void)derive_key(
        label_enc,
        srtp_ctx_->key_ctx.master.remote_key,
        srtp_ctx_->key_ctx.master.remote_salt,
        srtp_ctx_->key_ctx.remote.enc_key,
        key_size
    );
    (void)derive_key(
        label_auth,
        srtp_ctx_->key_ctx.master.remote_key,
        srtp_ctx_->key_ctx.master.remote_salt,
        srtp_ctx_->key_ctx.remote.auth_key,
        UVG_AUTH_LENGTH
    );
    (void)derive_key(
        label_salt,
        srtp_ctx_->key_ctx.master.remote_key,
        srtp_ctx_->key_ctx.master.remote_salt,
        srtp_ctx_->key_ctx.remote.salt_key,
        UVG_SALT_LENGTH
    );

    return ret;
}

rtp_error_t uvgrtp::base_srtp::allocate_crypto_ctx(size_t key_size)
{
    srtp_ctx_->key_ctx.master.local_key = new uint8_t[key_size];
    srtp_ctx_->key_ctx.master.remote_key = new uint8_t[key_size];

    srtp_ctx_->key_ctx.local.enc_key = new uint8_t[key_size];
    srtp_ctx_->key_ctx.remote.enc_key = new uint8_t[key_size];

    return RTP_OK;
}

size_t uvgrtp::base_srtp::get_key_size(int flags)
{
    size_t key_size = AES128_KEY_SIZE;

    if (!(flags & RCE_SRTP_KMNGMNT_ZRTP))
    {
        if (flags & RCE_SRTP_KEYSIZE_192)
            key_size = AES192_KEY_SIZE;
        else if (flags & RCE_SRTP_KEYSIZE_256)
            key_size = AES256_KEY_SIZE;
    }

    return key_size;
}

rtp_error_t uvgrtp::base_srtp::set_master_keys(size_t key_size, uint8_t* local_key, uint8_t* remote_key,
    uint8_t* local_salt, uint8_t* remote_salt)
{
  if (!local_key || !remote_key || !local_salt || !remote_salt)
    return RTP_INVALID_VALUE;

  rtp_error_t ret = RTP_OK;
  if ((ret = allocate_crypto_ctx(key_size)) != RTP_OK)
      return ret;

  memcpy(srtp_ctx_->key_ctx.master.local_key,    local_key,   key_size);
  memcpy(srtp_ctx_->key_ctx.master.remote_key,   remote_key,  key_size);
  memcpy(srtp_ctx_->key_ctx.master.local_salt,   local_salt,  UVG_SALT_LENGTH);
  memcpy(srtp_ctx_->key_ctx.master.remote_salt,  remote_salt, UVG_SALT_LENGTH);

  return ret;
}
