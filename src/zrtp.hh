#pragma once

#ifdef _WIN32
#include <winsock2.h>
#include <mswsock.h>
#include <inaddr.h>
#else
#include <netinet/ip.h>
#include <arpa/inet.h>
#endif

#include <vector>

#include "crypto/crypto.hh"
#include "mzrtp/defines.hh"
#include "mzrtp/receiver.hh"

namespace kvz_rtp {

    enum ROLE {
        INITIATOR,
        RESPONDER
    };

    /* TODO: move to defines.hh */
    typedef struct capabilities {

        /* Supported ZRTP version */
        uint32_t version;

        /* ZID of this ZRTP instance */
        uint8_t *zid;

        /* Header of the supported algos etc. */
        uint32_t header;

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

    /* DH exchange related information */
    typedef struct zrtp_dh {
        /* uint32_t retained1[2]; /1* hash of retained shared secret 1 *1/ */
        /* uint32_t retained2[2]; /1* hash of retained shared secret 2 *1/ */
        /* uint32_t aux_secret[2]; /1* hash of auxiliary secret *1/ */
        /* uint32_t pbx_secret[2]; /1* hash of MiTM PBX secret *1/ */

        /* Retained (for kvzRTP, preshared mode is not supported so we're
         * going to generate just some random values for these) */
        uint8_t rs1[32];
        uint8_t rs2[32];
        uint8_t raux[32];
        uint8_t rpbx[32];

        /* Shared between  parties */
        uint8_t s1[8];
        uint8_t s2[8];
        uint8_t aux[8];
        uint8_t pbx[8];

    } zrtp_dh_t;

    /* One common crypto contex for all ZRTP functions */
    typedef struct zrtp_crypto_ctx {
        kvz_rtp::crypto::hmac::sha256 *hmac_sha256;
        kvz_rtp::crypto::sha256 *sha256;
        kvz_rtp::crypto::dh *dh;
    } zrtp_crypto_ctx_t;

    typedef struct zrtp_messages {
        std::pair<size_t, struct kvz_rtp::zrtp_msg::zrtp_confirm *> confirm;
        std::pair<size_t, struct kvz_rtp::zrtp_msg::zrtp_commit  *> commit;
        std::pair<size_t, struct kvz_rtp::zrtp_msg::zrtp_hello   *> hello;
        std::pair<size_t, struct kvz_rtp::zrtp_msg::zrtp_dh      *> dh;
    } zrtp_messages_t;

    /* TODO: voisiko näitä structeja järjestellä jotenkin järkevämmin? */

    /* Collection of algorithms that are used by ZRTP
     * (based on information gathered from Hello message) */
    typedef struct zrtp_session {
        uint32_t ssrc;
        uint16_t seq;

        uint32_t hash_algo;
        uint32_t cipher_algo;
        uint32_t auth_tag_type;
        uint32_t key_agreement_type;
        uint32_t sas_type;

        /* Session capabilities (our and theirs) */
        zrtp_capab_t capabilities; /* TODO: rename to ocapab */
        zrtp_capab_t rcapab;

        uint8_t private_key[22];
        uint8_t public_key[384];

        uint8_t remote_public[384];

        uint8_t dh_result[384];

        /* Hash value of initiator */
        uint8_t hvi[32];
        uint8_t remote_hvi[32];

        /* Section 9 of RFC 6189 */
        uint8_t hashes[4][32];
        uint8_t remote_hashes[4][32];

        uint64_t remote_macs[4];
        uint64_t confirm_mac;

        uint8_t total_hash[32];

        uint8_t zrtp_sess_key[32];
        uint8_t sas_hash[32];
        uint8_t zrtp_keyi[16];
        uint8_t zrtp_keyr[16];
        uint8_t hmac_keyi[32];
        uint8_t hmac_keyr[32];

        /* TODO: zid initiator */
        /* TODO: zid responder */

        /* Shared secrets
         *
         * Because kvzRTP supports only DH mode,
         * other shared secrets (s1 - s3) are null */
        uint8_t s0[32];
        uint8_t *s1;
        uint8_t *s2;
        uint8_t *s3;

        zrtp_dh_t us;
        zrtp_dh_t them;

        /* Are we the initiator or the responder? */
        int role;

        uint8_t remote_zid[12];

        /* Used by message classes */
        zrtp_crypto_ctx_t cctx;

        /* Pointers to messages sent by us and messages received from remote.
         * These are just to calculate various hash values */
        zrtp_messages_t l_msg;
        zrtp_messages_t r_msg;

    } zrtp_session_t;

    class zrtp {
        public:
            zrtp();
            ~zrtp();

            /* Initialize ZRTP session between us and remote
             *
             * Return RTP_OK on success
             * Return RTP_TIMEOUT if remote did not send messages in timely manner */
            rtp_error_t init(uint32_t ssrc, socket_t& socket, sockaddr_in& addr);

        private:
            /* Set timeout for a socket, needed by backoff timers of ZRTP
             *
             * "timeout" tells the timeout in milliseconds
             *
             * Return RTP_OK on success
             * Return RTP_GENERIC_ERROR if timeout could not be set */
            rtp_error_t set_timeout(size_t timeout);

            /* Get our own capabilities, see struct capabilities above for more details */
            zrtp_capab_t get_capabilities();

            /* Generate zid for this ZRTP instance. ZID is a unique, 96-bit long ID */
            void generate_zid();

            /* Create private/public key pair and generate random values for retained secrets */
            void generate_secrets();

            /* Calculate DHResult, total_hash, and s0 according to rules defined in RFC 6189 */
            void generate_shared_secrets();

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
            rtp_error_t init_session();

            /* Perform Diffie-Hellman key exchange Part1 (responder)
             * This message also acts as an ACK to Commit message */
            rtp_error_t dh_part1();

            /* Perform Diffie-Hellman key exchange Part2 (initiator)
             * This message also acts as an ACK to DHPart1 message
             *
             * Return RTP_OK if DHPart2 was successful
             * Return RTP_TIMEOUT if no message is received from remote before T2 expires */
            rtp_error_t dh_part2();

            /* TODO:  */
            rtp_error_t calculate_shared_secret();

            /* TODO:  */
            rtp_error_t responder_finalize_session();

            /* TODO:  */
            rtp_error_t initiator_finalize_session();

            uint32_t ssrc_;
            socket_t socket_;
            sockaddr_in addr_;

            /* Our own and remote capability structs */
            zrtp_capab_t capab_;
            zrtp_capab_t rcapab_;

            kvz_rtp::zrtp_msg::receiver receiver_;

            zrtp_crypto_ctx_t cctx_;
            /* Initialized after Hello messages have been exchanged */
            zrtp_session_t session_;

            uint8_t *zid_;
    };
};
