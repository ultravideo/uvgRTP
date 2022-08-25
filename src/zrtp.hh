#pragma once

#include "zrtp/zrtp_receiver.hh"
#include "zrtp/defines.hh"


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
#include <memory>


namespace uvgrtp {

    namespace frame {
        struct rtp_frame;
    }

    enum ZRTP_ROLE {
        INITIATOR,
        RESPONDER
    };

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
            rtp_error_t init(uint32_t ssrc, std::shared_ptr<uvgrtp::socket> socket, sockaddr_in& addr, bool perform_dh);

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
                uint8_t *our_mkey,    uint32_t okey_len,
                uint8_t *their_mkey,  uint32_t tkey_len,
                uint8_t *our_msalt,   uint32_t osalt_len,
                uint8_t *their_msalt, uint32_t tsalt_len
            );

            /* ZRTP packet handler is used after ZRTP state initialization has finished
             * and media exchange has started. RTP reception flow gives the packet
             * to "zrtp_handler" which then checks whether the packet is a ZRTP packet
             * or not and processes it accordingly.
             *
             * Return RTP_OK on success
             * Return RTP_PKT_NOT_HANDLED if "buffer" does not contain a ZRTP message
             * Return RTP_GENERIC_ERROR if "buffer" contains an invalid ZRTP message */
            static rtp_error_t packet_handler(ssize_t size, void *packet, int rce_flags, frame::rtp_frame **out);

        private:
            /* Initialize ZRTP session between us and remote using Diffie-Hellman Mode
             *
             * Return RTP_OK on success
             * Return RTP_TIMEOUT if remote did not send messages in timely manner */
            rtp_error_t init_dhm(uint32_t ssrc, std::shared_ptr<uvgrtp::socket> socket, sockaddr_in& addr);

            /* Initialize ZRTP session between us and remote using Multistream mode
             *
             * Return RTP_OK on success
             * Return RTP_TIMEOUT if remote did not send messages in timely manner */
            rtp_error_t init_msm(uint32_t ssrc, std::shared_ptr<uvgrtp::socket> socket, sockaddr_in& addr);

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
            std::shared_ptr<uvgrtp::socket> local_socket_;
            sockaddr_in remote_addr_;

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
}

namespace uvg_rtp = uvgrtp;
