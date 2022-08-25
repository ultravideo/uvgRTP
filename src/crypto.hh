#pragma once

#if __cplusplus >= 201703L || _MSC_VER >= 1911
#if __has_include(<cryptopp/aes.h>) && \
    __has_include(<cryptopp/base32.h>) && \
    __has_include(<cryptopp/cryptlib.h>) && \
    __has_include(<cryptopp/dh.h>) && \
    __has_include(<cryptopp/hmac.h>) && \
    __has_include(<cryptopp/modes.h>) && \
    __has_include(<cryptopp/osrng.h>) && \
    __has_include(<cryptopp/sha.h>) && \
    __has_include(<cryptopp/crc.h>) && \
    !defined(__RTP_NO_CRYPTO__)

#define __RTP_CRYPTO__

#include <cryptopp/aes.h>
#include <cryptopp/base32.h>
#include <cryptopp/cryptlib.h>
#include <cryptopp/dh.h>
#include <cryptopp/hmac.h>
#include <cryptopp/modes.h>
#include <cryptopp/osrng.h>
#include <cryptopp/sha.h>
#include <cryptopp/crc.h>

#endif
#else // __cplusplus < 201703L
#ifndef __RTP_NO_CRYPTO__
#define __RTP_CRYPTO__

#include <cryptopp/aes.h>
#include <cryptopp/base32.h>
#include <cryptopp/cryptlib.h>
#include <cryptopp/dh.h>
#include <cryptopp/hmac.h>
#include <cryptopp/modes.h>
#include <cryptopp/osrng.h>
#include <cryptopp/sha.h>
#include <cryptopp/crc.h>

#endif
#endif // __cplusplus

#include <iostream>

namespace uvgrtp {

    namespace crypto {

        /* hash-based message authentication code */
        namespace hmac {
            class sha1 {
                public:
                    sha1(const uint8_t *key, size_t key_size);
                    ~sha1();

                    void update(const uint8_t *data, size_t len);
                    void final(uint8_t *digest);

                    /* truncate digest to "size" bytes */
                    void final(uint8_t *digest, size_t size);

                private:
#ifdef __RTP_CRYPTO__
                    CryptoPP::HMAC<CryptoPP::SHA1> hmac_;
#endif
            };

            class sha256 {
                public:
                    sha256(const uint8_t *key, size_t key_size);
                    ~sha256();

                    void update(const uint8_t *data, size_t len);
                    void final(uint8_t *digest);

                private:
#ifdef __RTP_CRYPTO__
                    CryptoPP::HMAC<CryptoPP::SHA256> hmac_;
#endif
            };
        }

        class sha256 {
            public:
                sha256();
                ~sha256();

                void update(const uint8_t *data, size_t len);
                void final(uint8_t *digest);

            private:
#ifdef __RTP_CRYPTO__
                CryptoPP::SHA256 sha_;
#endif
        };

        namespace aes {

            class ecb {
                public:
                    ecb(const uint8_t *key, size_t key_size);
                    ~ecb();

                    void encrypt(uint8_t *output, const uint8_t *input, size_t len);
                    void decrypt(uint8_t *output, const uint8_t *input, size_t len);

                private:
#ifdef __RTP_CRYPTO__
                    CryptoPP::ECB_Mode<CryptoPP::AES>::Encryption enc_;
                    CryptoPP::ECB_Mode<CryptoPP::AES>::Decryption dec_;
#endif
            };

            class cfb {
                public:
                    cfb(const uint8_t *key, size_t key_size, const uint8_t *iv);
                    ~cfb();

                    void encrypt(uint8_t *output, const uint8_t *input, size_t len);
                    void decrypt(uint8_t *output, const uint8_t *input, size_t len);

                private:
#ifdef __RTP_CRYPTO__
                    CryptoPP::CFB_Mode<CryptoPP::AES>::Encryption enc_;
                    CryptoPP::CFB_Mode<CryptoPP::AES>::Decryption dec_;
#endif
            };

            class ctr {
                public:
                    ctr(const uint8_t *key, size_t key_size, const uint8_t *iv);
                    ~ctr();

                    void encrypt(uint8_t *output, const uint8_t *input, size_t len);
                    void decrypt(uint8_t *output, const uint8_t *input, size_t len);

                private:
#ifdef __RTP_CRYPTO__
                    CryptoPP::CTR_Mode<CryptoPP::AES>::Encryption enc_;
                    CryptoPP::CTR_Mode<CryptoPP::AES>::Decryption dec_;
#endif
            };
        }

        /* diffie-hellman key derivation, 3072-bits */
        class dh {
            public:
                dh();
                ~dh();

                void generate_keys();
                void get_pk(uint8_t *pk, size_t len);
                void set_remote_pk(uint8_t *pk, size_t len);
                void get_shared_secret(uint8_t *ss, size_t len);

            private:
#ifdef __RTP_CRYPTO__
                CryptoPP::AutoSeededRandomPool prng_;
                CryptoPP::DH dh_;
                CryptoPP::Integer sk_, pk_, rpk_;
#endif
        };

        /* base32 */
        class b32 {
            public:
                b32();
                ~b32();

                void encode(const uint8_t *input, uint8_t *output, size_t len);

            private:
#ifdef __RTP_CRYPTO__
                CryptoPP::Base32Encoder enc_;
#endif
        };

        namespace random {
            void generate_random(uint8_t *out, size_t len);
        }

        namespace crc32 {
            void get_crc32(const uint8_t *input, size_t len, uint32_t *output);
            bool verify_crc32(const uint8_t *input, size_t len, uint32_t old_crc);
            uint32_t calculate_crc32(const uint8_t *input, size_t len);
        }

        bool enabled();
    }
}

namespace uvg_rtp = uvgrtp;
