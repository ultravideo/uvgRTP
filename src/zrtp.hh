#pragma once

#include "zrtp/zrtp_receiver.hh"
#include "zrtp/defines.hh"
#include "zrtp/commit.hh"
#include "zrtp/confack.hh"
#include "zrtp/confirm.hh"
#include "zrtp/dh_kxchng.hh"
#include "zrtp/hello.hh"
#include "zrtp/hello_ack.hh"

#ifdef _WIN32
#include <winsock2.h>
#include <mswsock.h>
#include <inaddr.h>
#include <ws2ipdef.h>

#else
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
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
            rtp_error_t init(uint32_t ssrc, std::shared_ptr<uvgrtp::socket> socket, sockaddr_in& addr, sockaddr_in6& addr6, bool perform_dh, bool ipv6);

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

            /* Handler for ZRTP packets
             *
             * Return RTP_OK on success
             * Return RTP_PKT_NOT_HANDLED if "buffer" does not contain a ZRTP message
             * Return RTP_GENERIC_ERROR if "buffer" contains an invalid ZRTP message */
            rtp_error_t packet_handler(void* args, int rce_flags, uint8_t* read_ptr, size_t size, frame::rtp_frame** out);

            inline bool has_dh_finished() const
            {
                return dh_finished_;
            }

            inline void dh_has_finished()
            {
                dh_finished_ = true;
            }

            inline bool is_zrtp_busy() const
            {
                return zrtp_busy_;
            }
            inline void set_zrtp_busy(bool status)
            {
                zrtp_busy_ = status;
            }

        private:
            /* Initialize ZRTP session between us and remote using Diffie-Hellman Mode
             *
             * Return RTP_OK on success
             * Return RTP_TIMEOUT if remote did not send messages in timely manner */
            rtp_error_t init_dhm(uint32_t ssrc, std::shared_ptr<uvgrtp::socket> socket, sockaddr_in& addr, sockaddr_in6& addr6, bool ipv6);

            /* Initialize ZRTP session between us and remote using Multistream mode
             *
             * Return RTP_OK on success
             * Return RTP_TIMEOUT if remote did not send messages in timely manner */
            rtp_error_t init_msm(uint32_t ssrc, std::shared_ptr<uvgrtp::socket> socket, sockaddr_in& addr, sockaddr_in6& addr6);

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

            void cleanup_session();

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
            rtp_error_t dh_part2(uvgrtp::zrtp_msg::dh_key_exchange* dh);

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
            sockaddr_in6 remote_ip6_addr_;

            /* Has the ZRTP connection been initialized using DH */
            bool initialized_;

            /* Our own and remote capability structs */
            zrtp_capab_t capab_;
            zrtp_capab_t rcapab_;

            zrtp_crypto_ctx_t cctx_;
            zrtp_session_t session_;

            std::mutex zrtp_mtx_;

            uvgrtp::zrtp_msg::zrtp_hello* hello_;
            uvgrtp::zrtp_msg::zrtp_hello_ack* hello_ack_;
            uvgrtp::zrtp_msg::zrtp_commit* commit_;
            uvgrtp::zrtp_msg::zrtp_dh* dh1_;
            uvgrtp::zrtp_msg::zrtp_dh* dh2_;
            uvgrtp::zrtp_msg::zrtp_confirm* conf1_;
            uvgrtp::zrtp_msg::zrtp_confirm* conf2_;
            uvgrtp::zrtp_msg::zrtp_confack* confack_;

            size_t hello_len_;
            size_t commit_len_;
            size_t dh_len_;

            std::mutex state_mutex_;
            bool dh_finished_ = false;
            bool zrtp_busy_;

    };
}

namespace uvg_rtp = uvgrtp;
