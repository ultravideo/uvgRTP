#ifdef __RTP_CRYPTO__
#include "crypto.hh"

#include <iostream>

/* ***************** hmac-sha1 ***************** */

kvz_rtp::crypto::hmac::sha1::sha1(uint8_t *key, size_t key_size):
    hmac_(key, key_size)
{
}

kvz_rtp::crypto::hmac::sha1::~sha1()
{
}

void kvz_rtp::crypto::hmac::sha1::update(uint8_t *data, size_t len)
{
    hmac_.Update(data, len);
}

void kvz_rtp::crypto::hmac::sha1::final(uint8_t *digest)
{
    hmac_.Final(digest);
}

/* ***************** hmac-sha256 ***************** */

kvz_rtp::crypto::hmac::sha256::sha256(uint8_t *key, size_t key_size):
    hmac_(key, key_size)
{
}

kvz_rtp::crypto::hmac::sha256::~sha256()
{
}

void kvz_rtp::crypto::hmac::sha256::update(uint8_t *data, size_t len)
{
    hmac_.Update(data, len);
}

void kvz_rtp::crypto::hmac::sha256::final(uint8_t *digest)
{
    hmac_.Final(digest);
}

/* ***************** sha256 ***************** */

kvz_rtp::crypto::sha256::sha256():
    sha_()
{
}

kvz_rtp::crypto::sha256::~sha256()
{
}

void kvz_rtp::crypto::sha256::update(uint8_t *data, size_t len)
{
    sha_.Update(data, len);
}

void kvz_rtp::crypto::sha256::final(uint8_t *digest)
{
    sha_.Final(digest);
}

/* ***************** aes-128 ***************** */

kvz_rtp::crypto::aes::ctr::ctr(uint8_t *key, size_t key_size, uint8_t *iv):
    enc_(key, key_size, iv),
    dec_(key, key_size, iv)
{
}

kvz_rtp::crypto::aes::ctr::~ctr()
{
}

void kvz_rtp::crypto::aes::ctr::encrypt(uint8_t *input, uint8_t *output, size_t len)
{
    enc_.ProcessData(input, output, len);
}

void kvz_rtp::crypto::aes::ctr::decrypt(uint8_t *input, uint8_t *output, size_t len)
{
    dec_.ProcessData(input, output, len);
}

kvz_rtp::crypto::aes::cfb::cfb(uint8_t *key, size_t key_size, uint8_t *iv):
    enc_(key, key_size, iv),
    dec_(key, key_size, iv)
{
}

kvz_rtp::crypto::aes::cfb::~cfb()
{
}

void kvz_rtp::crypto::aes::cfb::encrypt(uint8_t *output, uint8_t *input, size_t len)
{
    enc_.ProcessData(output, input, len);
}

void kvz_rtp::crypto::aes::cfb::decrypt(uint8_t *output, uint8_t *input, size_t len)
{
    dec_.ProcessData(output, input, len);
}

/* ***************** diffie-hellman 3072 ***************** */

kvz_rtp::crypto::dh::dh():
    prng_(),
    dh_(),
    rpk_()
{
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
}

kvz_rtp::crypto::dh::~dh()
{
}

void kvz_rtp::crypto::dh::generate_keys()
{
    CryptoPP::SecByteBlock t1(dh_.PrivateKeyLength()), t2(dh_.PublicKeyLength());
    dh_.GenerateKeyPair(prng_, t1, t2);

    sk_ = CryptoPP::Integer(t1, t1.size());
    pk_ = CryptoPP::Integer(t2, t2.size());
}

void kvz_rtp::crypto::dh::get_pk(uint8_t *pk, size_t len)
{
    pk_.Encode(pk, len);
}

void kvz_rtp::crypto::dh::set_remote_pk(uint8_t *pk, size_t len)
{
    rpk_.Decode(pk, len);
}

void kvz_rtp::crypto::dh::get_shared_secret(uint8_t *ss, size_t len)
{
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
}

/* ***************** base32 ***************** */
kvz_rtp::crypto::b32::b32():
    enc_()
{
}

kvz_rtp::crypto::b32::~b32()
{
}

void kvz_rtp::crypto::b32::encode(uint8_t *input, uint8_t *output, size_t len)
{
    enc_.Put(input, len);
    enc_.MessageEnd();

    CryptoPP::word64 max_ret = enc_.MaxRetrievable();

    if (max_ret) {
        enc_.Get(output, len);
    }
}

/* ***************** random ***************** */

void kvz_rtp::crypto::random::generate_random(uint8_t *out, size_t len)
{
    /* do not block ever */
    CryptoPP::OS_GenerateRandomBlock(false, out, len);
}

/* ***************** crc32 ***************** */

void kvz_rtp::crypto::crc32::get_crc32(uint8_t *input, size_t len, uint32_t *output)
{
    CryptoPP::CRC32 crc32;

    crc32.Update(input, len);
    crc32.TruncatedFinal((uint8_t *)output, sizeof(uint32_t));
}

bool kvz_rtp::crypto::crc32::verify_crc32(uint8_t *input, size_t len, uint32_t old_crc)
{
    CryptoPP::CRC32 crc32;
    uint32_t new_crc;

    crc32.Update(input, len);
    crc32.TruncatedFinal((uint8_t *)&new_crc, sizeof(uint32_t));

    return new_crc == old_crc;
}
#endif
