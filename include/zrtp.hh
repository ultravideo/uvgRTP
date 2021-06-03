#pragma once

#include "zrtp/zrtp_receiver.hh"


#ifdef _WIN32
#include <winsock2.h>
#include <mswsock.h>
#include <inaddr.h>
#else
#include <netinet/ip.h>
#include <arpa/inet.h>
#endif

#include <mutex>
#include <vector>



namespace uvgrtp {

    namespace frame {
        struct rtp_frame;
    };

    namespace crypto
    {
        namespace hmac {
            class sha256;
        };

        class sha256;
        class dh;
    };

    namespace zrtp_msg {
        struct zrtp_hello_ack;
        struct zrtp_commit;
        struct zrtp_hello;
        struct zrtp_dh;
    };

    enum ZRTP_ROLE {
        INITIATOR,
        RESPONDER
    };

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

	typedef struct zrtp_crypto_ctx {
        uvgrtp::crypto::hmac::sha256 *hmac_sha256 = nullptr;
        uvgrtp::crypto::sha256 *sha256 = nullptr;
        uvgrtp::crypto::dh *dh = nullptr;
    } zrtp_crypto_ctx_t;

    typedef struct zrtp_secrets {
        /* Retained (for uvgRTP, preshared mode is not supported so we're
         * going to generate just some random values for these) */
        uint8_t rs1[32];
        uint8_t rs2[32];
        uint8_t raux[32];
        uint8_t rpbx[32];

        /* Shared secrets
         *
         * Because uvgRTP supports only DH mode,
         * other shared secrets (s1 - s3) are null */
        uint8_t s0[32];
        uint8_t *s1 = nullptr;
        uint8_t *s2 = nullptr;
        uint8_t *s3 = nullptr;
    } zrtp_secrets_t;

    typedef struct zrtp_messages {
        std::pair<size_t, struct uvgrtp::zrtp_msg::zrtp_commit  *> commit;
        std::pair<size_t, struct uvgrtp::zrtp_msg::zrtp_hello   *> hello;
        std::pair<size_t, struct uvgrtp::zrtp_msg::zrtp_dh      *> dh;
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
        zrtp_capab_t capabilities;

        /* Various hash values of the ZRTP session */
        zrtp_hash_ctx_t hash_ctx;

        /* DH-related variables */
        zrtp_dh_ctx_t dh_ctx;

        /* ZRTP keying material (for HMAC/AES etc) */
        zrtp_key_ctx_t key_ctx;

        /* Retained and shared secrets of the ZRTP session */
        zrtp_secrets_t secrets;

        uint8_t o_zid[12]; /* our ZID */
        uint8_t r_zid[12]; /* remote ZID */

        /* Pointers to messages sent by us and messages received from remote.
         * These are used to calculate various hash values */
        zrtp_messages_t l_msg;
        zrtp_messages_t r_msg;
    } zrtp_session_t;

    class zrtp {
        public:
            zrtp();
            ~zrtp();

            /* Initialize ZRTP for a multimedia session
             *
             * If this the first ZRTP session initialization for this object,
             * ZRTP will perform DHMode initialization, otherwise Multistream Mode
             * initialization is performed.
             *
             * Return RTP_OK on success
             * Return RTP_TIMEOUT if remote did not send messages in timely manner */
            rtp_error_t init(uint32_t ssrc, uvgrtp::socket *socket, sockaddr_in& addr);

            /* Get SRTP keys for the session that was just initialized
             *
             * NOTE: "key_len" and "salt_len" denote the lengths in **bits**
             *
             * TODO are there any requirements (thinking of Multistream Mode and keys getting overwritten?)
             *
             * Return RTP_OK on success
             * Return RTP_NOT_INITIALIZED if init() has not been called yet
             * Return RTP_INVALID_VALUE if one of the parameters is invalid */
            rtp_error_t get_srtp_keys(
                uint8_t *our_mkey,    size_t okey_len,
                uint8_t *their_mkey,  size_t tkey_len,
                uint8_t *our_msalt,   size_t osalt_len,
                uint8_t *their_msalt, size_t tsalt_len
            );

            /* ZRTP packet handler is used after ZRTP state initialization has finished
             * and media exchange has started. RTP packet dispatcher gives the packet
             * to "zrtp_handler" which then checks whether the packet is a ZRTP packet
             * or not and processes it accordingly.
             *
             * Return RTP_OK on success
             * Return RTP_PKT_NOT_HANDLED if "buffer" does not contain a ZRTP message
             * Return RTP_GENERIC_ERROR if "buffer" contains an invalid ZRTP message */
            static rtp_error_t packet_handler(ssize_t size, void *packet, int flags, frame::rtp_frame **out);

        private:
            /* Initialize ZRTP session between us and remote using Diffie-Hellman Mode
             *
             * Return RTP_OK on success
             * Return RTP_TIMEOUT if remote did not send messages in timely manner */
            rtp_error_t init_dhm(uint32_t ssrc, uvgrtp::socket *socket, sockaddr_in& addr);

            /* Initialize ZRTP session between us and remote using Multistream mode
             *
             * Return RTP_OK on success
             * Return RTP_TIMEOUT if remote did not send messages in timely manner */
            rtp_error_t init_msm(uint32_t ssrc, uvgrtp::socket *socket, sockaddr_in& addr);

            /* Generate zid for this ZRTP instance. ZID is a unique, 96-bit long ID */
            void generate_zid();

            /* Create private/public key pair and generate random values for retained secrets */
            void generate_secrets();

            /* Calculate DHResult, total_hash, and s0
             * according to rules defined in RFC 6189 for Diffie-Hellman mode*/
            void generate_shared_secrets_dh();

            /* Calculate shared secrets for Multistream Mode */
            void generate_shared_secrets_msm();

            /* Compare our and remote's hvi values to determine who is the initiator */
            bool are_we_initiator(uint8_t *our_hvi, uint8_t *their_hvi);

            /* Initialize the four session hashes defined in Section 9 of RFC 6189 */
            void init_session_hashes();

            /* Derive new key using s0 as HMAC key */
            void derive_key(const char *label, uint32_t key_len, uint8_t *key);

            /* Being the ZRTP session by sending a Hello message to remote,
             * and responding to remote's Hello message using HelloAck message
             *
             * If session begins successfully, remote zrtp_capab_t are put into
             * "remote_capab" for later use
             *
             * Return RTP_OK on success
             * Return RTP_NOT_SUPPORTED if remote did not answer to our Hello messages */
            rtp_error_t begin_session();

            /* Select algorithms used by the session, exchange this information with remote
             * and based on Commit messages, select roles for the participants (initiator/responder)
             *
             * Return RTP_OK on success
             * Return RTP_TIMEOUT if no message is received from remote before T2 expires */
            rtp_error_t init_session(int key_agreement);

            /* Calculate HMAC-SHA256 using "key" for "buf" of "len" bytes
             * and compare the truncated, 64-bit hash digest against "mac".
             *
             * Return RTP_OK if they match
             * Return RTP_INVALID if they do not match */
            rtp_error_t verify_hash(uint8_t *key, uint8_t *buf, size_t len, uint64_t mac);

            /* Validate all received MACs and Hashes to make sure that we're really
             * talking with the correct person */
            rtp_error_t validate_session();

            /* Perform Diffie-Hellman key exchange Part1 (responder)
             * This message also acts as an ACK to Commit message */
            rtp_error_t dh_part1();

            /* Perform Diffie-Hellman key exchange Part2 (initiator)
             * This message also acts as an ACK to DHPart1 message
             *
             * Return RTP_OK if DHPart2 was successful
             * Return RTP_TIMEOUT if no message is received from remote before T2 expires */
            rtp_error_t dh_part2();

            /* Calculate all the shared secrets (f.ex. DHResult and ZRTP Session Keys) */
            rtp_error_t calculate_shared_secret();

            /* Finalize the session for responder by sending Confirm1 and Conf2ACK messages
             * Before this step validate_session() is called to make sure we have a valid session */
            rtp_error_t responder_finalize_session();

            /* Finalize the session for initiator by sending Confirm2 message
             * Before this step validate_session() is called to make sure we have a valid session */
            rtp_error_t initiator_finalize_session();

            uint32_t ssrc_;
            uvgrtp::socket *socket_;
            sockaddr_in addr_;

            /* Has the ZRTP connection been initialized using DH */
            bool initialized_;

            /* Our own and remote capability structs */
            zrtp_capab_t capab_;
            zrtp_capab_t rcapab_;

            /* ZRTP packet receiver */
            uvgrtp::zrtp_msg::receiver receiver_;

            zrtp_crypto_ctx_t cctx_;
            zrtp_session_t session_;

            std::mutex zrtp_mtx_;
    };
};

namespace uvg_rtp = uvgrtp;
