#ifdef __RTP_CRYPTO__
#pragma once

#include <cryptopp/aes.h>
#include <cryptopp/base32.h>
#include <cryptopp/cryptlib.h>
#include <cryptopp/dh.h>
#include <cryptopp/hmac.h>
#include <cryptopp/modes.h>
#include <cryptopp/osrng.h>
#include <cryptopp/sha.h>
#include <cryptopp/crc.h>

namespace uvg_rtp {

    namespace crypto {

        /* hash-based message authentication code */
        namespace hmac {
            class sha1 {
                public:
                    sha1(uint8_t *key, size_t key_size);
                    ~sha1();

                    void update(uint8_t *data, size_t len);
                    void final(uint8_t *digest);

                private:
                    CryptoPP::HMAC<CryptoPP::SHA1> hmac_;
            };

            class sha256 {
                public:
                    sha256(uint8_t *key, size_t key_size);
                    ~sha256();

                    void update(uint8_t *data, size_t len);
                    void final(uint8_t *digest);

                private:
                    CryptoPP::HMAC<CryptoPP::SHA256> hmac_;
            };
        };

        class sha256 {
            public:
                sha256();
                ~sha256();

                void update(uint8_t *data, size_t len);
                void final(uint8_t *digest);

            private:
                CryptoPP::SHA256 sha_;
        };

        namespace aes {

            class ecb {
                public:
                    ecb(uint8_t *key, size_t key_size);
                    ~ecb();

                    void encrypt(uint8_t *output, uint8_t *input, size_t len);
                    void decrypt(uint8_t *output, uint8_t *input, size_t len);

                private:
                    CryptoPP::ECB_Mode<CryptoPP::AES>::Encryption enc_;
                    CryptoPP::ECB_Mode<CryptoPP::AES>::Decryption dec_;
            };

            class cfb {
                public:
                    cfb(uint8_t *key, size_t key_size, uint8_t *iv);
                    ~cfb();

                    void encrypt(uint8_t *output, uint8_t *input, size_t len);
                    void decrypt(uint8_t *output, uint8_t *input, size_t len);

                private:
                    CryptoPP::CFB_Mode<CryptoPP::AES>::Encryption enc_;
                    CryptoPP::CFB_Mode<CryptoPP::AES>::Decryption dec_;
            };

            class ctr {
                public:
                    ctr(uint8_t *key, size_t key_size, uint8_t *iv);
                    ~ctr();

                    void encrypt(uint8_t *output, uint8_t *input, size_t len);
                    void decrypt(uint8_t *output, uint8_t *input, size_t len);

                private:
                    CryptoPP::CTR_Mode<CryptoPP::AES>::Encryption enc_;
                    CryptoPP::CTR_Mode<CryptoPP::AES>::Decryption dec_;
            };
        };

        /* diffie-hellman key derivation, 3072-bits */
        class dh {
            public:
                dh();
                ~dh();

                /* TODO:  */
                void generate_keys();

                /* TODO:  */
                void get_pk(uint8_t *pk, size_t len);

                /* TODO:  */
                void set_remote_pk(uint8_t *pk, size_t len);

                /* TODO:  */
                void get_shared_secret(uint8_t *ss, size_t len);

            private:
                CryptoPP::AutoSeededRandomPool prng_;
                CryptoPP::DH dh_;
                CryptoPP::Integer sk_, pk_, rpk_;
        };

        /* base32 */
        class b32 {
            public:
                b32();
                ~b32();

                void encode(uint8_t *input, uint8_t *output, size_t len);

            private:
                CryptoPP::Base32Encoder enc_;
        };

        namespace random {
            void generate_random(uint8_t *out, size_t len);
        };

        namespace crc32 {
            void get_crc32(uint8_t *input, size_t len, uint32_t *output);
            bool verify_crc32(uint8_t *input, size_t len, uint32_t old_crc);
        };
    };
};
#endif
