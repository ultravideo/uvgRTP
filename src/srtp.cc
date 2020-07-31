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

rtp_error_t uvg_rtp::srtp::__init(int type, int flags)
{
    srtp_ctx_.roc  = 0;
    srtp_ctx_.type = type;
    srtp_ctx_.enc  = AES_128;
    srtp_ctx_.hmac = HMAC_SHA1;

    srtp_ctx_.mki_size    = 0;
    srtp_ctx_.mki_present = false;
    srtp_ctx_.mki         = nullptr;

    srtp_ctx_.master_key  = key_ctx_.master.local_key;
    srtp_ctx_.master_salt = key_ctx_.master.local_salt;
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
        key_ctx_.master.local_key,
        key_ctx_.master.local_salt,
        key_ctx_.local.enc_key,
        AES_KEY_LENGTH
    );
    (void)derive_key(
        SRTP_AUTHENTICATION,
        key_ctx_.master.local_key,
        key_ctx_.master.local_salt,
        key_ctx_.local.auth_key,
        AES_KEY_LENGTH
    );
    (void)derive_key(
        SRTP_SALTING,
        key_ctx_.master.local_key,
        key_ctx_.master.local_salt,
        key_ctx_.local.salt_key,
        SALT_LENGTH
    );

    /* Remote aka decryption keys */
    (void)derive_key(
        SRTP_ENCRYPTION,
        key_ctx_.master.remote_key,
        key_ctx_.master.remote_salt,
        key_ctx_.remote.enc_key,
        AES_KEY_LENGTH
    );
    (void)derive_key(
        SRTP_AUTHENTICATION,
        key_ctx_.master.remote_key,
        key_ctx_.master.remote_salt,
        key_ctx_.remote.auth_key,
        AES_KEY_LENGTH
    );
    (void)derive_key(
        SRTP_SALTING,
        key_ctx_.master.remote_key,
        key_ctx_.master.remote_salt,
        key_ctx_.remote.salt_key,
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
        key_ctx_.master.local_key,   AES_KEY_LENGTH * 8,
        key_ctx_.master.remote_key,  AES_KEY_LENGTH * 8,
        key_ctx_.master.local_salt,  SALT_LENGTH    * 8,
        key_ctx_.master.remote_salt, SALT_LENGTH    * 8
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

    memcpy(key_ctx_.master.local_key,    key, AES_KEY_LENGTH);
    memcpy(key_ctx_.master.remote_key,   key, AES_KEY_LENGTH);
    memcpy(key_ctx_.master.local_salt,  salt, SALT_LENGTH);
    memcpy(key_ctx_.master.remote_salt, salt, SALT_LENGTH);

    return __init(type, flags);
}

rtp_error_t uvg_rtp::srtp::__encrypt(uint32_t ssrc, uint16_t seq, uint8_t *buffer, size_t len)
{
    if (use_null_cipher_)
        return RTP_OK;

    uint8_t iv[16] = { 0 };
    uint64_t index = (((uint64_t)srtp_ctx_.roc) << 16) + seq;

    /* Sequence number has wrapped around, update Roll-over Counter */
    if (seq == 0xffff)
        srtp_ctx_.roc++;

    if (create_iv(iv, ssrc, index, key_ctx_.local.salt_key) != RTP_OK) {
        LOG_ERROR("Failed to create IV, unable to encrypt the RTP packet!");
        return RTP_INVALID_VALUE;
    }

    uvg_rtp::crypto::aes::ctr ctr(key_ctx_.local.enc_key, sizeof(key_ctx_.local.enc_key), iv);
    ctr.encrypt(buffer, buffer, len);

    return RTP_OK;
}

rtp_error_t uvg_rtp::srtp::encrypt(uvg_rtp::frame::rtp_frame *frame)
{
    /* TODO: authentication for complete RTP frame requires co-operation
     * with the allocator of that frame -> no good solution for that right now */

    return __encrypt(
        ntohl(frame->header.ssrc),
        ntohs(frame->header.seq),
        frame->payload,
        frame->payload_len
    );
}

rtp_error_t uvg_rtp::srtp::encrypt(std::vector<std::pair<size_t, uint8_t *>>& buffers)
{
    auto frame       = (uvg_rtp::frame::rtp_frame *)buffers.at(0).second;
    auto rtp         = buffers.at(buffers.size() - 1);
    uint64_t *digest = new uint64_t;

    rtp_error_t ret = __encrypt(
        ntohl(frame->header.ssrc),
        ntohs(frame->header.seq),
        rtp.second,
        rtp.first
    );

    if (!authenticate_rtp_)
        return ret;

    /* create authentication tag for the packet and push it to the vector buffer */
    auto hmac_sha1 = uvg_rtp::crypto::hmac::sha1(key_ctx_.local.auth_key, AES_KEY_LENGTH);

    for (auto& buffer : buffers)
        hmac_sha1.update((uint8_t *)buffer.second, buffer.first);

    hmac_sha1.update((uint8_t *)&srtp_ctx_.roc, sizeof(srtp_ctx_.roc));
    hmac_sha1.final((uint8_t *)digest);

    buffers.push_back(std::make_pair(sizeof(uint64_t), (uint8_t *)digest));
    return ret;
}

rtp_error_t uvg_rtp::srtp::decrypt(uint8_t *buffer, size_t len)
{
    if (use_null_cipher_)
        return RTP_OK;

    uint8_t iv[16]  = { 0 };
    auto hdr        = (uvg_rtp::frame::rtp_header *)buffer;
    uint16_t seq    = ntohs(hdr->seq);
    uint32_t ssrc   = ntohl(hdr->ssrc);
    uint64_t index  = (((uint64_t)srtp_ctx_.roc) << 16) + seq;
    uint64_t digest = 0;

    /* Sequence number has wrapped around, update Roll-over Counter */
    if (seq == 0xffff)
        srtp_ctx_.roc++;

    if (create_iv(iv, ssrc, index, key_ctx_.remote.salt_key) != RTP_OK) {
        LOG_ERROR("Failed to create IV, unable to encrypt the RTP packet!");
        return RTP_INVALID_VALUE;
    }

    uint8_t *payload = buffer + sizeof(uvg_rtp::frame::rtp_header);
    uvg_rtp::crypto::aes::ctr ctr(key_ctx_.remote.enc_key, sizeof(key_ctx_.remote.enc_key), iv);

    /* exit early if RTP packet authentication is disabled... */
    if (!authenticate_rtp_) {
        ctr.decrypt(payload, payload, len - sizeof(uvg_rtp::frame::rtp_header));
        return RTP_OK;
    }

    /* ... otherwise calculate authentication tag for the packet
     * and compare it against the one we received */
    auto hmac_sha1 = uvg_rtp::crypto::hmac::sha1(key_ctx_.remote.auth_key, AES_KEY_LENGTH);

    hmac_sha1.update((uint8_t *)buffer, len - AUTH_TAG_LENGTH);
    hmac_sha1.update((uint8_t *)&srtp_ctx_.roc, sizeof(srtp_ctx_.roc));
    hmac_sha1.final((uint8_t *)&digest);

    if (memcmp(&digest, &buffer[len - AUTH_TAG_LENGTH], AUTH_TAG_LENGTH)) {
        LOG_ERROR("Authentication tag mismatch!");
        return RTP_AUTH_TAG_MISMATCH;
    }

    size_t size_ = len - sizeof(uvg_rtp::frame::rtp_header) - 8;
    uint8_t *new_buffer = new uint8_t[size_];

    ctr.decrypt(new_buffer, payload, size_);
    memset(payload, 0, len);
    memcpy(payload, new_buffer, size_);
    return RTP_OK;
}
#endif
