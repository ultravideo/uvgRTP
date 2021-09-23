#pragma once

#include "util.hh"

#ifdef _WIN32
#include <winsock2.h>
#include <mswsock.h>
#include <inaddr.h>
#else
#include <netinet/ip.h>
#include <arpa/inet.h>
#endif

#include <cstdint>
#include <unordered_set>
#include <vector>



enum {
    AES128_KEY_SIZE = 16,
    AES192_KEY_SIZE = 24,
    AES256_KEY_SIZE = 32
};

#define UVG_AES_KEY_LENGTH      16 /* 128 bits */
#define UVG_HMAC_KEY_LENGTH     32 /* 256 bits */
#define UVG_SALT_LENGTH         14 /* 112 bits */
#define UVG_AUTH_LENGTH         16
#define UVG_IV_LENGTH           16
#define UVG_AUTH_TAG_LENGTH     10
#define UVG_SRTCP_INDEX_LENGTH   4

namespace uvgrtp {

    /* Vector of buffers that contain a full RTP frame */
    typedef std::vector<std::pair<size_t, uint8_t *>> buf_vec;

    enum STYPE {
        SRTP  = 0,
        SRTCP = 1
    };

    enum ETYPE {
        AES_128 = 0,
        AES_192 = 1,
        AES_256 = 2
    };

    enum HTYPE {
        HMAC_SHA1 = 0,
    };

    enum LABELS {
        SRTP_ENCRYPTION      = 0x0,
        SRTP_AUTHENTICATION  = 0x1,
        SRTP_SALTING         = 0x2,
        SRTCP_ENCRYPTION     = 0x3,
        SRTCP_AUTHENTICATION = 0x4,
        SRTCP_SALTING        = 0x5
    };

    /* Key context for SRTP keys,
     * ZRTP generates two keys: one for initiator and one for responder
     *
     * Master key is not directly used to encrypt packets but it is used
     * to create session keys for the SRTP/SRTCP */
    typedef struct srtp_key_ctx {
        /* Keys negotiated by ZRTP */
        struct {
            /* Our master key and salt */
            uint8_t *local_key = nullptr;
            uint8_t local_salt[UVG_SALT_LENGTH];

            /* Remote's master key and salt */
            uint8_t *remote_key = nullptr;
            uint8_t remote_salt[UVG_SALT_LENGTH];
        } master;

        /* Used to encrypt/authenticate packets sent by us */
        struct {
            uint8_t *enc_key = nullptr;
            uint8_t auth_key[UVG_AUTH_LENGTH];
            uint8_t salt_key[UVG_SALT_LENGTH];
        } local;

        /* Used to decrypt/Authenticate packets sent by remote */
        struct {
            uint8_t *enc_key = nullptr;
            uint8_t auth_key[UVG_AUTH_LENGTH];
            uint8_t salt_key[UVG_SALT_LENGTH];
        } remote;

    } srtp_key_ctx_t;

    typedef struct srtp_ctx {
        int type = 0;     /* srtp or srtcp */
        uint32_t roc = 0; /* roll-over counter */
        uint32_t rts = 0; /* timestamp of the frame that causes ROC update */

        int enc = 0;   /* identifier for encryption algorithm */
        int hmac = 0;  /* identifier for message authentication algorithm */

        bool mki_present = 0; /* is MKI present in SRTP packets */
        size_t mki_size = 0;  /* length of the MKI field in bytes if it's present */
        uint8_t *mki = nullptr;     /* master key identifier */

        uint8_t *master_key = nullptr;  /* master key */
        uint8_t *master_salt = nullptr; /* master salt */
        size_t  mk_cnt = 0;       /* how many packets have been encrypted with master key */

        size_t n_e = 0; /* size of encryption key */
        size_t n_a = 0; /* size of hmac key */

        /* following fields are receiver-only */
        uint16_t s_l = 0;    /* highest received sequence number */
        uint8_t *replay = nullptr; /* list of recently received and authenticated SRTP packets */

        int flags = 0; /* context configuration flags */

        srtp_key_ctx_t key_ctx;
    } srtp_ctx_t;

    class base_srtp {
        public:
            base_srtp();
            virtual ~base_srtp();

            /* Setup Secure RTP/RTCP connection with master keys
             *
             * Length of the "key" must be either 128, 192, or 256 bits
             * Length of "salt" must be SALT_LENGTH (14 bytes, 112 bits)
             *
             * Return RTP_OK if SRTP setup was successful
             * Return RTP_INVALID_VALUE if "key" or "salt" is nullptr
             * Return RTP_MEMORY allocation failed */
            rtp_error_t init(int type, int flags, uint8_t *local_key, uint8_t *remote_key,
                             uint8_t *local_salt, uint8_t *remote_salt);

            /* Has RTP packet encryption been disabled? */
            bool use_null_cipher();


            /* Get reference to the SRTP context (including session keys) */
            srtp_ctx_t *get_ctx();

            /* Returns true if the packet having this HMAC digest is replayed
             * Returns false if replay protection has not been enabled */
            bool is_replayed_packet(uint8_t *digest);

            size_t get_key_size(int flags);

        protected:

            /* Create IV for the packet that is about to be encrypted
             *
             * Return RTP_OK on success and place the iv to "out"
             * Return RTP_INVALID_VALUE if one of the parameters is invalid */
            rtp_error_t create_iv(uint8_t *out, uint32_t ssrc, uint64_t index, uint8_t *salt);

            /* SRTP context containing all session information and keys */
            srtp_ctx_t *srtp_ctx_;

            /* If NULL cipher is enabled, it means that RTP packets are not
             * encrypted but other security mechanisms described in RFC 3711 may be used */
            bool use_null_cipher_;

        private:
            rtp_error_t set_master_keys(size_t key_size, uint8_t* local_key, uint8_t* remote_key,
                                 uint8_t* local_salt, uint8_t* remote_salt);

            rtp_error_t derive_key(int label, uint8_t *key, uint8_t *salt, uint8_t *out, size_t len);

            /* Allocate space for master/session encryption keys */
            rtp_error_t allocate_crypto_ctx(size_t key_size);


            /* Map containing all authentication tags of received packets (separate for SRTP and SRTCP)
             * Used to implement replay protection */
            std::unordered_set<uint64_t> replay_list_;
    };
};

namespace uvg_rtp = uvgrtp;
