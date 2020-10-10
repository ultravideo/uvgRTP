#include <iostream>
#include "crypto.hh"
#include "debug.hh"

/* ***************** hmac-sha1 ***************** */

uvg_rtp::crypto::hmac::sha1::sha1(uint8_t *key, size_t key_size)
#ifdef __RTP_CRYPTO__
    :hmac_(key, key_size)
#endif
{
#ifndef __RTP_CRYPTO__
    (void)key, (void)key_size;
#endif
}

uvg_rtp::crypto::hmac::sha1::~sha1()
{
}

void uvg_rtp::crypto::hmac::sha1::update(uint8_t *data, size_t len)
{
#ifdef __RTP_CRYPTO__
    hmac_.Update(data, len);
#else
    (void)data, (void)len;

    LOG_ERROR("Recompile uvgRTP with -D__RTP_CRYPTO__");
    exit(EXIT_FAILURE);
#endif
}

void uvg_rtp::crypto::hmac::sha1::final(uint8_t *digest)
{
#ifdef __RTP_CRYPTO__
    hmac_.Final(digest);
#else
    (void)digest;

    LOG_ERROR("Recompile uvgRTP with -D__RTP_CRYPTO__");
    exit(EXIT_FAILURE);
#endif
}

void uvg_rtp::crypto::hmac::sha1::final(uint8_t *digest, size_t size)
{
#ifdef __RTP_CRYPTO__
    uint8_t d[20] = { 0 };

    hmac_.Final(d);
    memcpy(digest, d, size);
#else
    (void)digest, (void)size;

    LOG_ERROR("Recompile uvgRTP with -D__RTP_CRYPTO__");
    exit(EXIT_FAILURE);
#endif
}

/* ***************** hmac-sha256 ***************** */

uvg_rtp::crypto::hmac::sha256::sha256(uint8_t *key, size_t key_size)
#ifdef __RTP_CRYPTO__
    :hmac_(key, key_size)
#endif
{
#ifndef __RTP_CRYPTO__
    (void)key, (void)key_size;
#endif
}

uvg_rtp::crypto::hmac::sha256::~sha256()
{
}

void uvg_rtp::crypto::hmac::sha256::update(uint8_t *data, size_t len)
{
#ifdef __RTP_CRYPTO__
    hmac_.Update(data, len);
#else
    (void)data, (void)len;

    LOG_ERROR("Recompile uvgRTP with -D__RTP_CRYPTO__");
    exit(EXIT_FAILURE);
#endif
}

void uvg_rtp::crypto::hmac::sha256::final(uint8_t *digest)
{
#ifdef __RTP_CRYPTO__
    hmac_.Final(digest);
#else
    (void)digest;

    LOG_ERROR("Recompile uvgRTP with -D__RTP_CRYPTO__");
    exit(EXIT_FAILURE);
#endif
}

/* ***************** sha256 ***************** */

uvg_rtp::crypto::sha256::sha256()
#ifdef __RTP_CRYPTO__
    :sha_()
#endif
{
}

uvg_rtp::crypto::sha256::~sha256()
{
}

void uvg_rtp::crypto::sha256::update(uint8_t *data, size_t len)
{
#ifdef __RTP_CRYPTO__
    sha_.Update(data, len);
#else
    (void)data, (void)len;

    LOG_ERROR("Recompile uvgRTP with -D__RTP_CRYPTO__");
    exit(EXIT_FAILURE);
#endif
}

void uvg_rtp::crypto::sha256::final(uint8_t *digest)
{
#ifdef __RTP_CRYPTO__
    sha_.Final(digest);
#else
    (void)digest;

    LOG_ERROR("Recompile uvgRTP with -D__RTP_CRYPTO__");
    exit(EXIT_FAILURE);
#endif
}

/* ***************** aes-128 ***************** */

uvg_rtp::crypto::aes::ctr::ctr(uint8_t *key, size_t key_size, uint8_t *iv)
#ifdef __RTP_CRYPTO__
    :enc_(key, key_size, iv),
    dec_(key, key_size, iv)
#endif
{
#ifndef __RTP_CRYPTO__
    (void)key, (void)key_size, (void)iv;
#endif
}

uvg_rtp::crypto::aes::ctr::~ctr()
{
}

void uvg_rtp::crypto::aes::ctr::encrypt(uint8_t *output, uint8_t *input, size_t len)
{
#ifdef __RTP_CRYPTO__
    enc_.ProcessData(output, input, len);
#else
    (void)output, (void)input, (void)len;

    LOG_ERROR("Recompile uvgRTP with -D__RTP_CRYPTO__");
    exit(EXIT_FAILURE);
#endif
}

void uvg_rtp::crypto::aes::ctr::decrypt(uint8_t *output, uint8_t *input, size_t len)
{
#ifdef __RTP_CRYPTO__
    dec_.ProcessData(output, input, len);
#else
    (void)output, (void)input, (void)len;

    LOG_ERROR("Recompile uvgRTP with -D__RTP_CRYPTO__");
    exit(EXIT_FAILURE);
#endif
}

uvg_rtp::crypto::aes::cfb::cfb(uint8_t *key, size_t key_size, uint8_t *iv)
#ifdef __RTP_CRYPTO__
    :enc_(key, key_size, iv),
    dec_(key, key_size, iv)
#endif
{
#ifndef __RTP_CRYPTO__
    (void)key, (void)key_size, (void)iv;
#endif
}

uvg_rtp::crypto::aes::cfb::~cfb()
{
}

void uvg_rtp::crypto::aes::cfb::encrypt(uint8_t *output, uint8_t *input, size_t len)
{
#ifdef __RTP_CRYPTO__
    enc_.ProcessData(output, input, len);
#else
    (void)output, (void)input, (void)len;

    LOG_ERROR("Recompile uvgRTP with -D__RTP_CRYPTO__");
    exit(EXIT_FAILURE);
#endif
}

void uvg_rtp::crypto::aes::cfb::decrypt(uint8_t *output, uint8_t *input, size_t len)
{
#ifdef __RTP_CRYPTO__
    dec_.ProcessData(output, input, len);
#else
    (void)output, (void)input, (void)len;

    LOG_ERROR("Recompile uvgRTP with -D__RTP_CRYPTO__");
    exit(EXIT_FAILURE);
#endif
}

uvg_rtp::crypto::aes::ecb::ecb(uint8_t *key, size_t key_size)
#ifdef __RTP_CRYPTO__
    :enc_(key, key_size),
    dec_(key, key_size)
#endif
{
#ifndef __RTP_CRYPTO__
    (void)key, (void)key_size;
#endif
}

uvg_rtp::crypto::aes::ecb::~ecb()
{
}

void uvg_rtp::crypto::aes::ecb::encrypt(uint8_t *output, uint8_t *input, size_t len)
{
#ifdef __RTP_CRYPTO__
    enc_.ProcessData(output, input, len);
#else
    (void)output, (void)input, (void)len;

    LOG_ERROR("Recompile uvgRTP with -D__RTP_CRYPTO__");
    exit(EXIT_FAILURE);
#endif
}

void uvg_rtp::crypto::aes::ecb::decrypt(uint8_t *output, uint8_t *input, size_t len)
{
#ifdef __RTP_CRYPTO__
    dec_.ProcessData(output, input, len);
#else
    (void)output, (void)input, (void)len;

    LOG_ERROR("Recompile uvgRTP with -D__RTP_CRYPTO__");
    exit(EXIT_FAILURE);
#endif
}

/* ***************** diffie-hellman 3072 ***************** */

uvg_rtp::crypto::dh::dh()
#ifdef __RTP_CRYPTO__
    :prng_(),
    dh_(),
    rpk_()
#endif
{
#ifdef __RTP_CRYPTO__
    CryptoPP::Integer p(
        "0xFFFFFFFFFFFFFFFFC90FDAA22168C234C4C6628B80DC1CD1"
        "29024E088A67CC74020BBEA63B139B22514A08798E3404DD"
        "EF9519B3CD3A431B302B0A6DF25F14374FE1356D6D51C245"
        "E485B576625E7EC6F44C42E9A637ED6B0BFF5CB6F406B7ED"
        "EE386BFB5A899FA5AE9F24117C4B1FE649286651ECE45B3D"
        "C2007CB8A163BF0598DA48361C55D39A69163FA8FD24CF5F"
        "83655D23DCA3AD961C62F356208552BB9ED529077096966D"
        "670C354E4ABC9804F1746C08CA18217C32905E462E36CE3B"
        "E39E772C180E86039B2783A2EC07A28FB5C55DF06F4C52C9"
        "DE2BCBF6955817183995497CEA956AE515D2261898FA0510"
        "15728E5A8AAAC42DAD33170D04507A33A85521ABDF1CBA64"
        "ECFB850458DBEF0A8AEA71575D060C7DB3970F85A6E1E4C7"
        "ABF5AE8CDB0933D71E8C94E04A25619DCEE3D2261AD2EE6B"
        "F12FFA06D98A0864D87602733EC86A64521F2B18177B200C"
        "BBE117577A615D6C770988C0BAD946E208E24FA074E5AB31"
        "43DB5BFCE0FD108E4B82D120A93AD2CAFFFFFFFFFFFFFFFF"
    );

    CryptoPP::Integer g("0x02");

    dh_.AccessGroupParameters().Initialize(p, g);
#else
    LOG_ERROR("Recompile uvgRTP with -D__RTP_CRYPTO__");
    exit(EXIT_FAILURE);
#endif
}

uvg_rtp::crypto::dh::~dh()
{
}

void uvg_rtp::crypto::dh::generate_keys()
{
#ifdef __RTP_CRYPTO__
    CryptoPP::SecByteBlock t1(dh_.PrivateKeyLength()), t2(dh_.PublicKeyLength());
    dh_.GenerateKeyPair(prng_, t1, t2);

    sk_ = CryptoPP::Integer(t1, t1.size());
    pk_ = CryptoPP::Integer(t2, t2.size());
#else
    LOG_ERROR("Recompile uvgRTP with -D__RTP_CRYPTO__");
    exit(EXIT_FAILURE);
#endif
}

void uvg_rtp::crypto::dh::get_pk(uint8_t *pk, size_t len)
{
#ifdef __RTP_CRYPTO__
    pk_.Encode(pk, len);
#else
    (void)pk, (void)len;

    LOG_ERROR("Recompile uvgRTP with -D__RTP_CRYPTO__");
    exit(EXIT_FAILURE);
#endif
}

void uvg_rtp::crypto::dh::set_remote_pk(uint8_t *pk, size_t len)
{
#ifdef __RTP_CRYPTO__
    rpk_.Decode(pk, len);
#else
    (void)pk, (void)len;

    LOG_ERROR("Recompile uvgRTP with -D__RTP_CRYPTO__");
    exit(EXIT_FAILURE);
#endif
}

void uvg_rtp::crypto::dh::get_shared_secret(uint8_t *ss, size_t len)
{
#ifdef __RTP_CRYPTO__
    CryptoPP::Integer p(
        "0xFFFFFFFFFFFFFFFFC90FDAA22168C234C4C6628B80DC1CD1"
        "29024E088A67CC74020BBEA63B139B22514A08798E3404DD"
        "EF9519B3CD3A431B302B0A6DF25F14374FE1356D6D51C245"
        "E485B576625E7EC6F44C42E9A637ED6B0BFF5CB6F406B7ED"
        "EE386BFB5A899FA5AE9F24117C4B1FE649286651ECE45B3D"
        "C2007CB8A163BF0598DA48361C55D39A69163FA8FD24CF5F"
        "83655D23DCA3AD961C62F356208552BB9ED529077096966D"
        "670C354E4ABC9804F1746C08CA18217C32905E462E36CE3B"
        "E39E772C180E86039B2783A2EC07A28FB5C55DF06F4C52C9"
        "DE2BCBF6955817183995497CEA956AE515D2261898FA0510"
        "15728E5A8AAAC42DAD33170D04507A33A85521ABDF1CBA64"
        "ECFB850458DBEF0A8AEA71575D060C7DB3970F85A6E1E4C7"
        "ABF5AE8CDB0933D71E8C94E04A25619DCEE3D2261AD2EE6B"
        "F12FFA06D98A0864D87602733EC86A64521F2B18177B200C"
        "BBE117577A615D6C770988C0BAD946E208E24FA074E5AB31"
        "43DB5BFCE0FD108E4B82D120A93AD2CAFFFFFFFFFFFFFFFF"
    );

    CryptoPP::ModularArithmetic ma(p);
    CryptoPP::Integer dhres = ma.Exponentiate(rpk_, sk_);

    dhres.Encode(ss, len);
#else
    (void)ss, (void)len;

    LOG_ERROR("Recompile uvgRTP with -D__RTP_CRYPTO__");
    exit(EXIT_FAILURE);
#endif
}

/* ***************** base32 ***************** */
uvg_rtp::crypto::b32::b32()
#ifdef __RTP_CRYPTO__
    :enc_()
#endif
{
}

uvg_rtp::crypto::b32::~b32()
{
}

void uvg_rtp::crypto::b32::encode(uint8_t *input, uint8_t *output, size_t len)
{
#ifdef __RTP_CRYPTO__
    enc_.Put(input, len);
    enc_.MessageEnd();

    CryptoPP::word64 max_ret = enc_.MaxRetrievable();

    if (max_ret) {
        enc_.Get(output, len);
    }
#else
    (void)input, (void)output, (void)len;

    LOG_ERROR("Recompile uvgRTP with -D__RTP_CRYPTO__");
    exit(EXIT_FAILURE);
#endif
}

/* ***************** random ***************** */

void uvg_rtp::crypto::random::generate_random(uint8_t *out, size_t len)
{
#ifdef __RTP_CRYPTO__
    /* do not block ever */
    CryptoPP::OS_GenerateRandomBlock(false, out, len);
#else
    (void)out, (void)len;

    LOG_ERROR("Recompile uvgRTP with -D__RTP_CRYPTO__");
    exit(EXIT_FAILURE);
#endif
}

/* ***************** crc32 ***************** */

void uvg_rtp::crypto::crc32::get_crc32(uint8_t *input, size_t len, uint32_t *output)
{
#ifdef __RTP_CRYPTO__
    CryptoPP::CRC32 crc32;

    crc32.Update(input, len);
    crc32.TruncatedFinal((uint8_t *)output, sizeof(uint32_t));
#else
    (void)input, (void)len, (void)output;

    LOG_ERROR("Recompile uvgRTP with -D__RTP_CRYPTO__");
    exit(EXIT_FAILURE);
#endif
}

uint32_t uvg_rtp::crypto::crc32::calculate_crc32(uint8_t *input, size_t len)
{
#ifdef __RTP_CRYPTO__
    CryptoPP::CRC32 crc32;
    uint32_t out;

    crc32.Update(input, len);
    crc32.TruncatedFinal((uint8_t *)&out, sizeof(uint32_t));

    return out;
#else
    (void)input, (void)len;

    LOG_ERROR("Recompile uvgRTP with -D__RTP_CRYPTO__");
    exit(EXIT_FAILURE);
#endif
}

bool uvg_rtp::crypto::crc32::verify_crc32(uint8_t *input, size_t len, uint32_t old_crc)
{
#ifdef __RTP_CRYPTO__
    CryptoPP::CRC32 crc32;
    uint32_t new_crc;

    crc32.Update(input, len);
    crc32.TruncatedFinal((uint8_t *)&new_crc, sizeof(uint32_t));

    return new_crc == old_crc;
#else
    (void)input, (void)len, (void)old_crc;

    LOG_ERROR("Recompile uvgRTP with -D__RTP_CRYPTO__");
    exit(EXIT_FAILURE);
#endif
}

bool uvg_rtp::crypto::enabled()
{
#ifdef __RTP_CRYPTO__
    return true;
#else
    return false;
#endif
}
