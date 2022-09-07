#pragma once

#include "uvgrtp/frame.hh"

#include <vector>

namespace uvgrtp {

    namespace zrtp_msg {

        struct zrtp_hello_ack;
        struct zrtp_commit;
        struct zrtp_hello;
        struct zrtp_dh;

        PACK(struct zrtp_header {
            uint16_t version:4;
            uint16_t unused:12;
            uint16_t seq = 0;
            uint32_t magic = 0;
            uint32_t ssrc = 0;
        });

        PACK(struct zrtp_msg {
            struct zrtp_header header;
            uint16_t preamble = 0;
            uint16_t length = 0;
            uint64_t msgblock = 0;
        });

        enum ZRTP_FRAME_TYPE {
            ZRTP_FT_HELLO     =  1,
            ZRTP_FT_HELLO_ACK =  2,
            ZRTP_FT_COMMIT    =  3,
            ZRTP_FT_DH_PART1  =  4,
            ZRTP_FT_DH_PART2  =  5,
            ZRTP_FT_CONFIRM1  =  6,
            ZRTP_FT_CONFIRM2  =  7,
            ZRTP_FT_CONF2_ACK =  8,
            ZRTP_FT_SAS_RELAY =  9,
            ZRTP_FT_RELAY_ACK = 10,
            ZRTP_FT_ERROR     = 11,
            ZRTP_FT_ERROR_ACK = 12,
            ZRTP_FT_PING_ACK  = 13
        };

#ifdef _MSC_VER
        enum ZRTP_MSG_TYPE: __int64 {
#else
        enum ZRTP_MSG_TYPE {
#endif
            ZRTP_MSG_HELLO     = 0x2020206f6c6c6548,
            ZRTP_MSG_HELLO_ACK = 0x4b43416f6c6c6548,
            ZRTP_MSG_COMMIT    = 0x202074696d6d6f43,
            ZRTP_MSG_DH_PART1  = 0x2031747261504844,
            ZRTP_MSG_DH_PART2  = 0x2032747261504844,
            ZRTP_MSG_CONFIRM1  = 0x316d7269666e6f43,
            ZRTP_MSG_CONFIRM2  = 0x326d7269666e6f43,
            ZRTP_MSG_CONF2_ACK = 0x4b434132666e6f43,
            ZRTP_MSG_ERROR     = 0x202020726f727245,
            ZRTP_MSG_ERROR_ACK = 0x4b4341726f727245,
            ZRTP_MSG_GO_CLEAR  = 0x207261656c436f47,
            ZRTP_MSG_CLEAR_ACK = 0x4b43417261656c43,
            ZRTP_MSG_SAS_RELAY = 0x79616c6572534153,
            ZRTP_MSG_RELAY_ACK = 0x4b434179616c6552,
            ZRTP_MSG_PING      = 0x20202020676e6950,
            ZRTP_MSG_PING_ACK  = 0x204b4341676e6950,
        };

        constexpr uint32_t ZRTP_MAGIC = 0x5a525450;
        constexpr uint16_t ZRTP_PREAMBLE = 0x505a;

        enum HASHES {
            S256 = 0x36353253,
            S384 = 0x34383353,
            N256 = 0x3635324e,
            N384 = 0x3438334e
        };

        enum CIPHERS {
            AES1 = 0x31534541,
            AES2 = 0x32534541,
            AES3 = 0x33534541,
            TFS1 = 0x31534632,
            TFS2 = 0x32534632,
            TFS3 = 0x33534632
        };

        enum AUTH_TAGS {
            HS32 = 0x32335348,
            HS80 = 0x30385348,
            SK32 = 0x32334b53,
            SK64 = 0x34364b53
        };

        enum KEY_AGREEMENT {
            DH3k = 0x6b334844,
            DH2k = 0x6b324844,
            EC25 = 0x35324345,
            EC38 = 0x38334345,
            EC52 = 0x32354345,
            PRSH = 0x68737250,
            MULT = 0x746c754d
        };

        enum SAS_TYPES {
            B32  = 0x20323342,
            B256 = 0x36353242
        };

        enum ERRORS {
            ZRTP_ERR_MALFORED_PKT        = 0x10,  /* Malformed packet */
            ZRTP_ERR_SOFTWARE            = 0x20,  /* Critical software error */
            ZRTP_ERR_VERSION             = 0x30,  /* Unsupported version */
            ZRTP_ERR_COMPONENT_MISMATCH  = 0x40,  /* Hello message component mismatch */
            ZRTP_ERR_NS_HASH_TYPE        = 0x51,  /* Hash type not supported */
            ZRTP_ERR_NS_CIPHER_TYPE      = 0x52,  /* Cipher type not supported  */
            ZRTP_ERR_NS_PBKEY_EXCHANGE   = 0x53,  /* Public key exchange not supported */
            ZRTP_ERR_NS_SRTP_AUTH_TAG    = 0x54,  /* SRTP auth tag not supported */
            ZRTP_ERR_NS_SAS_RENDERING    = 0x55,  /* SAS Rendering Scheme not supported */
            ZRTP_ERR_NO_SHARED_SECRET    = 0x56,  /* No shared secret available */
            ZRTP_ERR_DHE_BAD_PVI         = 0x61,  /* DH Error: Bad pvi or pvr */
            ZRTP_ERR_DHE_HVI_MISMATCH    = 0x62,  /* DH Error: hvi != hashed data */
            ZRTP_ERR_UNTRUSTED_MITM      = 0x63,  /* Received relayed SAS from untrusted MiTM */
            ZRTP_ERR_BAD_CONFIRM_MAC     = 0x70,  /* Bad Confirm Packet MAC */
            ZRTP_ERR_NONCE_REUSE         = 0x80,  /* Nonce reuse */
            ZRTP_ERR_EQUAL_ZID           = 0x90,  /* Equal ZID in Hello */
            ZRTP_ERR_SSRC_COLLISION      = 0x91,  /* SSRC collision */
            ZRTP_ERR_SERVICE_UNAVAILABLE = 0xA0,  /* Service unavailable */
            ZRTP_ERR_PROTOCOL_TIMEOUT    = 0xB0,  /* Protocol timeout error */
            ZRTP_ERR_GOCLEAR_NOT_ALLOWED = 0x100, /* Goclear received but not supported */
        };
    }


    typedef struct capabilities {
        /* Supported ZRTP version */
        uint32_t version = 0;

        /* Supported hash algorithms (empty for us) */
        std::vector<uint32_t> hash_algos;

        /* Supported cipher algorithms (empty for us) */
        std::vector<uint32_t> cipher_algos;

        /* Supported authentication tag types (empty for us) */
        std::vector<uint32_t> auth_tags;

        /* Supported Key Agreement types (empty for us) */
        std::vector<uint32_t> key_agreements;

        /* Supported SAS types (empty for us) */
        std::vector<uint32_t> sas_types;
    } zrtp_capab_t;

    namespace crypto
    {
        namespace hmac {
            class sha256;
        }

        class sha256;
        class dh;
    }

    typedef struct zrtp_crypto_ctx {
        uvgrtp::crypto::hmac::sha256* hmac_sha256 = nullptr;
        uvgrtp::crypto::sha256* sha256 = nullptr;
        uvgrtp::crypto::dh* dh = nullptr;
    } zrtp_crypto_ctx_t;

    typedef struct zrtp_secrets {
        /* Retained (for uvgRTP, preshared mode is not supported so we're
         * going to generate just some random values for these) */
        uint8_t rs1[32] = {};
        uint8_t rs2[32] = {};
        uint8_t raux[32] = {};
        uint8_t rpbx[32] = {};

        /* Shared secrets
         *
         * Because uvgRTP supports only DH mode,
         * other shared secrets (s1 - s3) are null */
        uint8_t s0[32] = {};
        uint8_t* s1 = nullptr;
        uint8_t* s2 = nullptr;
        uint8_t* s3 = nullptr;
    } zrtp_secrets_t;

    typedef struct zrtp_messages {
        std::pair<size_t, struct uvgrtp::zrtp_msg::zrtp_commit*> commit = {0, nullptr};
        std::pair<size_t, struct uvgrtp::zrtp_msg::zrtp_hello*>  hello  = {0, nullptr};
        std::pair<size_t, struct uvgrtp::zrtp_msg::zrtp_dh*>     dh     = {0, nullptr};
    } zrtp_messages_t;

    /* Various ZRTP-related keys */
    typedef struct zrtp_key_ctx {
        uint8_t zrtp_sess_key[32];
        uint8_t sas_hash[32];

        /* ZRTP keys used to encrypt Confirm1/Confirm2 messages */
        uint8_t zrtp_keyi[16];
        uint8_t zrtp_keyr[16];

        /* HMAC keys used to authenticate Confirm1/Confirm2 messages */
        uint8_t hmac_keyi[32];
        uint8_t hmac_keyr[32];
    } zrtp_key_ctx_t;

    /* Diffie-Hellman context for the ZRTP session */
    typedef struct zrtp_dh_ctx {
        /* Our public/private key pair */
        uint8_t private_key[22];
        uint8_t public_key[384];

        /* Remote public key received in DHPart1/DHPart2 Message */
        uint8_t remote_public[384];

        /* DHResult aka "remote_public ^ private_key mod p" (see src/crypto/crypto.cc) */
        uint8_t dh_result[384];
    } zrtp_dh_ctx_t;

    typedef struct zrtp_hash_ctx {
        uint8_t o_hvi[32]; /* our hash value of initator (if we're the initiator) */
        uint8_t r_hvi[32]; /* remote's hash value of initiator (if they're the initiator) */

        /* Session hashes (H0 - H3), Section 9 of RFC 6189 */
        uint8_t o_hash[4][32]; /* our session hashes */
        uint8_t r_hash[4][32]; /* remote's session hashes */

        uint64_t r_mac[4];

        /* Section 4.4.1.4 */
        uint8_t total_hash[32];
    } zrtp_hash_ctx_t;

    /* Collection of algorithms that are used by ZRTP
     * (based on information gathered from Hello message) */
    typedef struct zrtp_session {
        int role = 0;       /* initiator/responder */
        uint32_t ssrc = 0;
        uint16_t seq = 0;

        uint32_t hash_algo = 0;
        uint32_t cipher_algo = 0;
        uint32_t auth_tag_type = 0;
        uint32_t key_agreement_type = 0;
        uint32_t sas_type = 0;

        /* Session capabilities */
        zrtp_capab_t capabilities = {};

        /* Various hash values of the ZRTP session */
        zrtp_hash_ctx_t hash_ctx = {};

        /* DH-related variables */
        zrtp_dh_ctx_t dh_ctx = {};

        /* ZRTP keying material (for HMAC/AES etc) */
        zrtp_key_ctx_t key_ctx = {};

        /* Retained and shared secrets of the ZRTP session */
        zrtp_secrets_t secrets = {};

        uint8_t o_zid[12]; /* our ZID */
        uint8_t r_zid[12]; /* remote ZID */

        /* Pointers to messages sent by us and messages received from remote.
         * These are used to calculate various hash values */
        zrtp_messages_t l_msg = {};
        zrtp_messages_t r_msg = {};
    } zrtp_session_t;


}

namespace uvg_rtp = uvgrtp;
