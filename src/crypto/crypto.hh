#pragma once

#include "crypto/3rdparty/cryptopp/aes.h"
#include "crypto/3rdparty/cryptopp/base32.h"
#include "crypto/3rdparty/cryptopp/cryptlib.h"
#include "crypto/3rdparty/cryptopp/dh.h"
#include "crypto/3rdparty/cryptopp/hmac.h"
#include "crypto/3rdparty/cryptopp/modes.h"
#include "crypto/3rdparty/cryptopp/osrng.h"
#include "crypto/3rdparty/cryptopp/sha.h"

namespace kvz_rtp {

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

        class aes {
            public:
                aes(uint8_t *key, size_t key_size, uint8_t *iv);
                ~aes();

                void encrypt(uint8_t *input, uint8_t *output, size_t len);
                void decrypt(uint8_t *input, uint8_t *output, size_t len);

            private:
                CryptoPP::CTR_Mode<CryptoPP::AES>::Encryption enc_;
                CryptoPP::CTR_Mode<CryptoPP::AES>::Decryption dec_;
        };

        /* diffie-hellman key derivation */
        class dh {
            public:
                dh();
                ~dh();

                /* TODO: generate keys? */

            private:
                CryptoPP::AutoSeededRandomPool prng_;
                CryptoPP::DH dh_;
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
    };
};
