#include "base.hh"

#include "../crypto.hh"
#include "../debug.hh"

#include <cstring>
#include <iostream>

uvgrtp::base_srtp::base_srtp():
    local_srtp_ctx_(std::shared_ptr<srtp_ctx_t>(new srtp_ctx_t)),
    remote_srtp_ctx_(std::shared_ptr<srtp_ctx_t>(new srtp_ctx_t)),
    use_null_cipher_(false)
{}

uvgrtp::base_srtp::~base_srtp()
{
    // cleanup keys
    cleanup_context(local_srtp_ctx_);
    cleanup_context(remote_srtp_ctx_);
}

bool uvgrtp::base_srtp::use_null_cipher()
{
    return use_null_cipher_;
}

std::shared_ptr<uvgrtp::srtp_ctx_t> uvgrtp::base_srtp::get_local_ctx()
{
    return local_srtp_ctx_;
}

std::shared_ptr<uvgrtp::srtp_ctx_t> uvgrtp::base_srtp::get_remote_ctx()
{
    return remote_srtp_ctx_;
}

rtp_error_t uvgrtp::base_srtp::derive_key(int label, size_t key_size, 
    uint8_t *key, uint8_t *salt, uint8_t *out, size_t out_len)
{
    uint8_t input[UVG_IV_LENGTH]    = { 0 };
    uint8_t ks[AES256_KEY_SIZE] = { 0 };

    memcpy(input, salt, UVG_SALT_LENGTH);
    memset(out, 0, out_len);

    input[7] ^= label;

    /* SRTP in uvgRTP uses ECB encryption for encrypting the session keys. For encrypting SRTP payloads, AES CTR mode is used.
     * ECB encryption is fine for encrypting short messages. However, using a different encryption method for
     * encrypting the keys too might be a more secure solution and should be explored. */

    uvgrtp::crypto::aes::ecb ecb(key, key_size); // srtp_ctx_->n_e);
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
    if (!(remote_srtp_ctx_->rce_flags & RCE_SRTP_REPLAY_PROTECTION))
        return false;

    uint64_t truncated;
    memcpy(&truncated, digest, sizeof(uint64_t));

    if (replay_list_.find(truncated) != replay_list_.end()) {
        UVG_LOG_ERROR("Replayed packet received, discarding!");
        return true;
    }

    replay_list_.insert(truncated);
    return false;
}

rtp_error_t uvgrtp::base_srtp::init(int type, int rce_flags, uint8_t* local_key, uint8_t* remote_key,
                                    uint8_t* local_salt, uint8_t* remote_salt)
{
    if (!local_key || !remote_key || !local_salt || !remote_salt)
        return RTP_INVALID_VALUE;

    use_null_cipher_ = (rce_flags & RCE_SRTP_NULL_CIPHER);

    init_srtp_context(local_srtp_ctx_,  type, rce_flags, local_key,  local_salt);
    init_srtp_context(remote_srtp_ctx_, type, rce_flags, remote_key, remote_salt);

    return RTP_OK;
}

uint32_t uvgrtp::base_srtp::get_key_size(int rce_flags) const
{
    uint32_t key_size = AES128_KEY_SIZE;

    if (!(rce_flags & RCE_SRTP_KMNGMNT_ZRTP))
    {
        if (rce_flags & RCE_SRTP_KEYSIZE_192)
            key_size = AES192_KEY_SIZE;
        else if (rce_flags & RCE_SRTP_KEYSIZE_256)
            key_size = AES256_KEY_SIZE;
    }

    return key_size;
}

rtp_error_t uvgrtp::base_srtp::init_srtp_context(std::shared_ptr<uvgrtp::srtp_ctx_t> context, int type, int rce_flags,
    uint8_t* key, uint8_t* salt)
{
    context->roc = 0;
    context->rts = 0;
    context->type = type;
    context->hmac = HMAC_SHA1;

    size_t key_size = get_key_size(rce_flags);

    switch (key_size) {
    case AES128_KEY_SIZE:
        context->enc = AES_128;
        break;

    case AES192_KEY_SIZE:
        context->enc = AES_192;
        break;

    case AES256_KEY_SIZE:
        context->enc = AES_256;
        break;
    }

    context->mki_size = 0;
    context->mki_present = false;
    context->mki = nullptr;
    context->mk_cnt = 0;

    context->n_e = key_size;
    context->n_a = UVG_HMAC_KEY_LENGTH;

    context->s_l = 0;
    context->replay = nullptr;
    context->rce_flags = rce_flags;

    int label_enc = 0;
    int label_auth = 0;
    int label_salt = 0;

    if (type == SRTP) {
        label_enc = SRTP_ENCRYPTION;
        label_auth = SRTP_AUTHENTICATION;
        label_salt = SRTP_SALTING;
    }
    else {
        label_enc = SRTCP_ENCRYPTION;
        label_auth = SRTCP_AUTHENTICATION;
        label_salt = SRTCP_SALTING;
    }

    context->master_key = new uint8_t[key_size];
    memcpy(context->master_key, key, key_size);
    memcpy(context->master_salt, salt, UVG_SALT_LENGTH);
    context->enc_key = new uint8_t[key_size]; // session key

    /* Derive session keys */
    (void)derive_key(
        label_enc,
        key_size,
        context->master_key,
        context->master_salt,
        context->enc_key,
        key_size
    );
    (void)derive_key(
        label_auth,
        key_size,
        context->master_key,
        context->master_salt,
        context->auth_key,
        UVG_AUTH_LENGTH
    );
    (void)derive_key(
        label_salt,
        key_size,
        context->master_key,
        context->master_salt,
        context->salt_key,
        UVG_SALT_LENGTH
    );

    return RTP_OK;
}

void uvgrtp::base_srtp::cleanup_context(std::shared_ptr<srtp_ctx_t> context)
{
    if (context)
    {
        if (context->master_key)
            delete[] context->master_key;
        if (context->enc_key)
            delete[] context->enc_key;
    }
}