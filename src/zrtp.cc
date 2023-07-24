#include "zrtp.hh"

#include "zrtp/commit.hh"
#include "zrtp/confack.hh"
#include "zrtp/confirm.hh"
#include "zrtp/dh_kxchng.hh"
#include "zrtp/hello.hh"
#include "zrtp/hello_ack.hh"

#include "socket.hh"
#include "crypto.hh"
#include "random.hh"
#include "debug.hh"
#include "uvgrtp/clock.hh"
#ifdef _WIN32
#include <ws2ipdef.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#endif
#include <cstring>
#include <thread>

using namespace uvgrtp::zrtp_msg;

#ifdef _WIN32
#define MSG_DONTWAIT 0
#endif


#define ZRTP_VERSION 110

uvgrtp::zrtp::zrtp():
    ssrc_(0),
    remote_addr_(),
    remote_ip6_addr_(),
    initialized_(false),
    hello_(nullptr),
    hello_ack_(nullptr),
    commit_(nullptr),
    dh1_(nullptr),
    dh2_(nullptr),
    conf1_(nullptr),
    conf2_(nullptr),
    confack_(nullptr),
    hello_len_(0),
    commit_len_(0),
    dh_len_(0),
    zrtp_busy_(false)
{
    cctx_.sha256 = new uvgrtp::crypto::sha256;
    cctx_.dh     = new uvgrtp::crypto::dh;
}

uvgrtp::zrtp::~zrtp()
{
    delete cctx_.sha256;
    delete cctx_.dh;

    cleanup_session();
}

void uvgrtp::zrtp::cleanup_session()
{
    if (session_.r_msg.commit.second)
    {
        delete[] session_.r_msg.commit.second;
                 session_.r_msg.commit.second = nullptr;
    }
    if (session_.r_msg.hello.second)
    {
        delete[] session_.r_msg.hello.second;
                 session_.r_msg.hello.second = nullptr;
    }
        
    if (session_.r_msg.dh.second)
    {
        delete[] session_.r_msg.dh.second;
                 session_.r_msg.dh.second = nullptr;
    }

    if (session_.l_msg.commit.second)
    {
        delete[] session_.l_msg.commit.second;
                 session_.l_msg.commit.second = nullptr;
    }

    if (session_.l_msg.hello.second)
    {
        delete[] session_.l_msg.hello.second;
                 session_.l_msg.hello.second = nullptr;
    }
    if (session_.l_msg.dh.second)
    {
        delete[] session_.l_msg.dh.second;
                 session_.l_msg.dh.second = nullptr;
    }
}

void uvgrtp::zrtp::generate_zid()
{
    uvgrtp::crypto::random::generate_random(session_.o_zid, 12);
}

/* ZRTP Key Derivation Function (KDF) (Section 4.5.2)
 *
 * KDF(KI, Label, Context, L) = HMAC(KI, i || Label || 0x00 || Context || L)
 *
 * Where:
 *    - KI      = s0
 *    - Label   = What is the key used for
 *    - Context = ZIDi || ZIDr || total_hash
 *    - L       = 256
 */
void uvgrtp::zrtp::derive_key(const char *label, uint32_t key_len, uint8_t *out_key)
{
    auto hmac_sha256 = uvgrtp::crypto::hmac::sha256(session_.secrets.s0, 32);
    uint8_t tmp[32]  = { 0 };
    uint32_t length  = htonl(key_len);
    uint32_t counter = 0x1;
    uint8_t delim    = 0x0;

    hmac_sha256.update((uint8_t *)&counter,  4);
    hmac_sha256.update((uint8_t *)label,     strlen(label));

    if (session_.role == INITIATOR) {
        hmac_sha256.update((uint8_t *)session_.o_zid, 12);
        hmac_sha256.update((uint8_t *)session_.r_zid, 12);
    } else {
        hmac_sha256.update((uint8_t *)session_.r_zid, 12);
        hmac_sha256.update((uint8_t *)session_.o_zid, 12);
    }

    hmac_sha256.update((uint8_t *)session_.hash_ctx.total_hash, 32);
    hmac_sha256.update((uint8_t *)&delim,                        1);
    hmac_sha256.update((uint8_t *)&length,                       4);

    /* Key length might differ from the digest length in which case the digest
     * must be generated to a temporary buffer and truncated to fit the "out_key" buffer */
    if (key_len != 256) {
        //UVG_LOG_DEBUG("Truncate key to %u bits!", key_len);

        hmac_sha256.final((uint8_t *)tmp);
        memcpy(out_key, tmp, key_len / 8);
    } else {
        hmac_sha256.final((uint8_t *)out_key);
    }
}

void uvgrtp::zrtp::generate_secrets()
{
    cctx_.dh->generate_keys();
    cctx_.dh->get_pk(session_.dh_ctx.public_key, 384);

    /* uvgRTP does not support Preshared mode (for now at least) so
     * there will be no shared secrets between the endpoints.
     *
     * Generate random data for the retained secret values that are sent
     * in the DHPart1/DHPart2 message and, due to mismatch, ignored by remote */
    uvgrtp::crypto::random::generate_random(session_.secrets.rs1,  32);
    uvgrtp::crypto::random::generate_random(session_.secrets.rs2,  32);
    uvgrtp::crypto::random::generate_random(session_.secrets.raux, 32);
    uvgrtp::crypto::random::generate_random(session_.secrets.rpbx, 32);
}

void uvgrtp::zrtp::generate_shared_secrets_dh()
{
    cctx_.dh->set_remote_pk(session_.dh_ctx.remote_public, 384);
    cctx_.dh->get_shared_secret(session_.dh_ctx.dh_result, 384);

    /* Section 4.4.1.4, calculation of total_hash includes:
     *    - Hello   (responder)
     *    - Commit  (initator)
     *    - DHPart1 (responder)
     *    - DHPart2 (initator)
     */
    if (session_.role == INITIATOR) {
        cctx_.sha256->update((uint8_t *)session_.r_msg.hello.second,  session_.r_msg.hello.first);
        cctx_.sha256->update((uint8_t *)session_.l_msg.commit.second, session_.l_msg.commit.first);
        cctx_.sha256->update((uint8_t *)session_.r_msg.dh.second,     session_.r_msg.dh.first);
        cctx_.sha256->update((uint8_t *)session_.l_msg.dh.second,     session_.l_msg.dh.first);
    } else {
        cctx_.sha256->update((uint8_t *)session_.l_msg.hello.second,  session_.l_msg.hello.first);
        cctx_.sha256->update((uint8_t *)session_.r_msg.commit.second, session_.r_msg.commit.first);
        cctx_.sha256->update((uint8_t *)session_.l_msg.dh.second,     session_.l_msg.dh.first);
        cctx_.sha256->update((uint8_t *)session_.r_msg.dh.second,     session_.r_msg.dh.first);
    }
    cctx_.sha256->final((uint8_t *)session_.hash_ctx.total_hash);

    /* Finally calculate s0 which is considered to be the final keying material (Section 4.4.1.4)
     *
     * It consist of the following information:
     *    - counter (always 1)
     *    - DHResult (calculated above [get_shared_secret()])
     *    - "ZRTP-HMAC-KDF"
     *    - ZID of initiator
     *    - ZID of responder
     *    - total hash (calculated above)
     *    - len(s1) (0x0)
     *    - s1 (null)
     *    - len(s2) (0x0)
     *    - s2 (null)
     *    - len(s3) (0x0)
     *    - s3 (null)
     */
    uint32_t value  = htonl(0x1);
    const char *kdf = "ZRTP-HMAC-KDF";

    cctx_.sha256->update((uint8_t *)&value,                    sizeof(value));              /* counter */
    cctx_.sha256->update((uint8_t *)session_.dh_ctx.dh_result, sizeof(session_.dh_ctx.dh_result));
    cctx_.sha256->update((uint8_t *)kdf,                       13);

    if (session_.role == INITIATOR) {
        cctx_.sha256->update((uint8_t *)session_.o_zid, 12);
        cctx_.sha256->update((uint8_t *)session_.r_zid, 12);
    } else {
        cctx_.sha256->update((uint8_t *)session_.r_zid, 12);
        cctx_.sha256->update((uint8_t *)session_.o_zid, 12);
    }

    cctx_.sha256->update((uint8_t *)session_.hash_ctx.total_hash, sizeof(session_.hash_ctx.total_hash));

    value = 0;
    cctx_.sha256->update((uint8_t *)&value, sizeof(value)); /* len(s1) */
    cctx_.sha256->update((uint8_t *)&value, sizeof(value)); /* len(s2) */
    cctx_.sha256->update((uint8_t *)&value, sizeof(value)); /* len(s3) */

    /* Calculate digest for s0 and erase DHResult from memory as required by the spec */
    cctx_.sha256->final((uint8_t *)session_.secrets.s0);
    memset(session_.dh_ctx.dh_result, 0, sizeof(session_.dh_ctx.dh_result));

    /* Derive ZRTP Session Key and SAS hash */
    derive_key("ZRTP Session Key", 256, session_.key_ctx.zrtp_sess_key);
    derive_key("SAS",              256, session_.key_ctx.sas_hash); /* TODO: crc32? */

    /* Finally derive ZRTP keys and HMAC keys
     * which are used to encrypt and authenticate Confirm messages */
    derive_key("Initiator ZRTP key", 128, session_.key_ctx.zrtp_keyi);
    derive_key("Responder ZRTP key", 128, session_.key_ctx.zrtp_keyr);
    derive_key("Initiator HMAC key", 256, session_.key_ctx.hmac_keyi);
    derive_key("Responder HMAC key", 256, session_.key_ctx.hmac_keyr);
}

void uvgrtp::zrtp::generate_shared_secrets_msm()
{
    if (session_.role == INITIATOR) {
        cctx_.sha256->update((uint8_t *)session_.r_msg.hello.second,  session_.r_msg.hello.first);
        cctx_.sha256->update((uint8_t *)session_.l_msg.commit.second, session_.l_msg.commit.first);
    } else {
        cctx_.sha256->update((uint8_t *)session_.l_msg.hello.second,  session_.l_msg.hello.first);
        cctx_.sha256->update((uint8_t *)session_.r_msg.commit.second, session_.r_msg.commit.first);
    }
    cctx_.sha256->final((uint8_t *)session_.hash_ctx.total_hash);

    /* Finally calculate s0 which is considered to be the final keying material (Section 4.4.3.2)
     *
     * It consist of the following information:
     *    - "ZRTP MSK"
     *    - ZID of initiator
     *    - ZID of responder
     *    - total hash (calculated above)
     *    - negotiated hash length (256)
     */
    uint32_t length = htonl(256);
    const char *kdf = "ZRTP MSK";

    cctx_.sha256->update((uint8_t *)kdf, strlen(kdf));

    if (session_.role == INITIATOR) {
        cctx_.sha256->update((uint8_t *)session_.o_zid, 12);
        cctx_.sha256->update((uint8_t *)session_.r_zid, 12);
    } else {
        cctx_.sha256->update((uint8_t *)session_.r_zid, 12);
        cctx_.sha256->update((uint8_t *)session_.o_zid, 12);
    }

    cctx_.sha256->update((uint8_t *)session_.hash_ctx.total_hash, sizeof(session_.hash_ctx.total_hash));
    cctx_.sha256->update((uint8_t *)&length, sizeof(length));

    /* Calculate digest for s0
     *
     * Caller can now generate SRTP session keys for the media stream */
    cctx_.sha256->final((uint8_t *)session_.secrets.s0);
}

rtp_error_t uvgrtp::zrtp::verify_hash(uint8_t *key, uint8_t *buf, size_t len, uint64_t mac)
{
    uint64_t truncated = 0;
    uint8_t full[32]   = { 0 };
    auto hmac_sha256   = uvgrtp::crypto::hmac::sha256(key, 32);

    hmac_sha256.update((uint8_t *)buf, len);
    hmac_sha256.final(full);

    memcpy(&truncated, full, sizeof(uint64_t));

    return (mac == truncated) ? RTP_OK : RTP_INVALID_VALUE;
}

rtp_error_t uvgrtp::zrtp::validate_session()
{
    /* Verify all MACs received from various messages in order starting from Hello message
     * Calculate HMAC-SHA256 over the saved message using H(i - 1) as the HMAC key and
     * compare the truncated hash against the hash was saved to the message */
    uint8_t hashes[4][32];
    memcpy(hashes[0], session_.hash_ctx.r_hash[0], 32);

    for (size_t i = 1; i < 4; ++i) {
        cctx_.sha256->update(hashes[i - 1], 32);
        cctx_.sha256->final(hashes[i]);
    }

    /* Hello message */
    if (RTP_INVALID_VALUE == verify_hash(
            (uint8_t *)hashes[2],
            (uint8_t *)session_.r_msg.hello.second,
            81,
            session_.hash_ctx.r_mac[3]
        ))
    {
        UVG_LOG_ERROR("Hash mismatch for Hello Message!");
        return RTP_INVALID_VALUE;
    }

    /* Check commit message only if our role is responder
     * because the initator might not have gotten a Commit message at all */
    if (session_.role == RESPONDER) {
        if (RTP_INVALID_VALUE == verify_hash(
                (uint8_t *)hashes[1],
                (uint8_t *)session_.r_msg.commit.second,
                session_.r_msg.commit.first - 8 - 4,
                session_.hash_ctx.r_mac[2]
            ))
        {
            UVG_LOG_ERROR("Hash mismatch for Commit Message!");
            return RTP_INVALID_VALUE;
        }
    }

    /* DHPart1/DHPart2 message */
    if (RTP_INVALID_VALUE == verify_hash(
            (uint8_t *)hashes[0],
            (uint8_t *)session_.r_msg.dh.second,
            session_.r_msg.dh.first - 8 - 4,
            session_.hash_ctx.r_mac[1]
        ))
    {
        UVG_LOG_ERROR("Hash mismatch for DHPart1/DHPart2 Message!");
        return RTP_INVALID_VALUE;
    }

    UVG_LOG_DEBUG("All hashes match!");
    return RTP_OK;
}

void uvgrtp::zrtp::init_session_hashes()
{
    uvgrtp::crypto::random::generate_random(session_.hash_ctx.o_hash[0], 32);

    for (size_t i = 1; i < 4; ++i) {
        cctx_.sha256->update(session_.hash_ctx.o_hash[i - 1], 32);
        cctx_.sha256->final(session_.hash_ctx.o_hash[i]);
    }
}

bool uvgrtp::zrtp::are_we_initiator(uint8_t *our_hvi, uint8_t *their_hvi)
{
    const int bits = (session_.key_agreement_type == MULT) ? 15 : 31;

    for (int i = bits; i >= 0; --i) {

        if (our_hvi[i] > their_hvi[i])
            return true;

        else if (our_hvi[i] < their_hvi[i])
            return false;
    }

    /* never here? */
    return true;
}

rtp_error_t uvgrtp::zrtp::begin_session()
{
    auto hello = uvgrtp::zrtp_msg::hello(session_);
    auto hello_ack = uvgrtp::zrtp_msg::hello_ack(session_);
    bool hello_recv = false;

    uvgrtp::clock::hrc::hrc_t start = uvgrtp::clock::hrc::now();
    int interval = 50;
    int i = 1;

    while (true) {

        /* We received something interesting, either Hello message from remote in which case
            * we need to send HelloACK message back and keep sending our Hello until HelloACK is received,
            * or HelloACK message which means we can stop sending our  */

            /* We received Hello message from remote, parse it and send  */
        if (hello_ != nullptr) {
            UVG_LOG_DEBUG("Got ZRTP Hello. Sending Hello ACK");
            hello_ack.send_msg(local_socket_, remote_addr_, remote_ip6_addr_);
            UVG_LOG_DEBUG("ZRTP HelloACK sent");

            if (!hello_recv) {
                hello_recv = true;

                /* Copy interesting information from receiver's
                    * message buffer to remote capabilities struct for later use */
                hello.parse_msg(hello_, session_, hello_len_);
                UVG_LOG_DEBUG("ZRTP Hello parsed");
                if (session_.capabilities.version != ZRTP_VERSION) {

                    /* Section 4.1.1:
                        *
                        * "If an endpoint receives a Hello message with an unsupported
                        *  version number that is lower than the endpoint's current Hello
                        *  message, the endpoint MUST send an Error message (Section 5.9)
                        *  indicating failure to support this ZRTP version."
                        */
                    if (session_.capabilities.version < ZRTP_VERSION) {
                        UVG_LOG_ERROR("Remote supports version %d, uvgRTP supports %d. Session cannot continue!",
                            session_.capabilities.version, ZRTP_VERSION);

                        return RTP_NOT_SUPPORTED;
                    }

                    UVG_LOG_WARN("ZRTP Protocol version %u not supported, keep sending Hello Messages",
                        session_.capabilities.version);
                    hello_recv = false;
                }
            }
            /* We received ACK for our Hello message.
                * Make sure we've received Hello message also before exiting */
        }
        if (hello_ack_ != nullptr) {

            if (hello_recv)
            {
                UVG_LOG_DEBUG("ZRTP Hello phase done");
                return RTP_OK;
            }
            else
            {
                UVG_LOG_DEBUG("Got Hello ACK without Hello!");
            }
        }
        long int next_sendslot = i * interval;
        long int run_time = (long int)uvgrtp::clock::hrc::diff_now(start);
        long int diff_ms = next_sendslot - run_time;

        if (diff_ms < 0) {
            UVG_LOG_DEBUG("Sending ZRTP hello # %i", i);
            if (hello.send_msg(local_socket_, remote_addr_, remote_ip6_addr_) != RTP_OK) {
                UVG_LOG_ERROR("Failed to send Hello message");
            }
            ++i;
            if (interval < 200) {
                interval *= 2;
            }
        }
        else {
            std::this_thread::sleep_for(std::chrono::milliseconds(diff_ms));
        }

        if (i > 20) {
            break;
        }
    }

    /* Hello timed out, perhaps remote did not answer at all or it has an incompatible ZRTP version in use. */
    return RTP_TIMEOUT;
}

rtp_error_t uvgrtp::zrtp::init_session(int key_agreement)
{
    /* Create ZRTP session from capabilities struct we've constructed */
    session_.hash_algo = S256;
    session_.cipher_algo = AES1;
    session_.auth_tag_type = HS32;
    session_.key_agreement_type = key_agreement;
    session_.sas_type = B32;

    auto commit = uvgrtp::zrtp_msg::commit(session_);

    /* First check if remote has already sent the message.
     * If so, they are the initiator and we're the responder */
    if (commit_ != nullptr) {
        commit.parse_msg(commit_, session_, commit_len_);
        session_.role = RESPONDER;
        return RTP_OK;
    }

    /* If we proceed to sending Commit message, we can assume we're the initiator.
     * This assumption may prove to be false if remote also sends Commit message
     * and Commit contention is resolved in their favor. */
    session_.role = INITIATOR;
    uvgrtp::clock::hrc::hrc_t start = uvgrtp::clock::hrc::now();
    int interval = 150;
    int i = 1;

    while (true) {  
        long int next_sendslot = i * interval;
        long int run_time = (long int)uvgrtp::clock::hrc::diff_now(start);
        long int diff_ms = next_sendslot - run_time;

        if (diff_ms < 0) {
            if (commit.send_msg(local_socket_, remote_addr_, remote_ip6_addr_) != RTP_OK) {
                UVG_LOG_ERROR("Failed to send Commit message!");
            }
            UVG_LOG_DEBUG("Commit sent");
            if (interval < 1200) {
                interval *= 2;
            }
            ++i;
        }
        if (commit_) {
            /* As per RFC 6189, if both parties have sent Commit message and the mode is DH,
             * hvi shall determine who is the initiator (the party with larger hvi is initiator) */
            commit.parse_msg(commit_, session_, commit_len_);

            /* Our hvi is smaller than remote's meaning we are the responder.
                *
                * Commit message must be ACKed with DHPart1 messages so we need exit,
                * construct that message and sent it to remote */
                
            if (!are_we_initiator(session_.hash_ctx.o_hvi, session_.hash_ctx.r_hvi)) {
                session_.role = RESPONDER;
                return RTP_OK;
            }
        }
        if (dh1_ || conf1_) {
            return RTP_OK;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        if (i > 10) {
            break;
        }
    }
    /* Remote didn't send us any messages, it can be considered dead
     * and ZRTP cannot thus continue any further */
    return RTP_TIMEOUT;
}

rtp_error_t uvgrtp::zrtp::dh_part1()
{
    auto dhpart = uvgrtp::zrtp_msg::dh_key_exchange(session_, 1);
    uvgrtp::clock::hrc::hrc_t start = uvgrtp::clock::hrc::now();
    int interval = 150;
    int i = 1;

    while (true) {

        if (dh2_ != nullptr) {
            UVG_LOG_DEBUG("DHPart2 found");

            if (dhpart.parse_msg(dh2_, session_, dh_len_) != RTP_OK) {
                UVG_LOG_ERROR("Failed to parse DHPart2 Message!");
                return RTP_GENERIC_ERROR;
            }
            UVG_LOG_DEBUG("DHPart2 received and parse successfully!");

            /* parse_msg() above extracted the public key of remote and saved it to session_.
                * Now we must generate shared secrets (DHResult, total_hash, and s0) */
            generate_shared_secrets_dh();
            return RTP_OK;
        }

        long int next_sendslot = i * interval;
        long int run_time = (long int)uvgrtp::clock::hrc::diff_now(start);
        long int diff_ms = next_sendslot - run_time;

        if (diff_ms < 0) {
            if (dhpart.send_msg(local_socket_, remote_addr_, remote_ip6_addr_) != RTP_OK) {
                UVG_LOG_ERROR("Failed to send DHPart1 Message!");
            }
            UVG_LOG_DEBUG("DHPart1 sent");
            if (interval < 1200) {
                interval *= 2;
            }
            ++i;
        }
        else {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));

        }
        if (i > 10) {
            break;
        }
    }
    return RTP_TIMEOUT;
}

rtp_error_t uvgrtp::zrtp::dh_part2(uvgrtp::zrtp_msg::dh_key_exchange* dh)
{
    rtp_error_t ret = RTP_OK;
    auto dhpart = dh;//uvgrtp::zrtp_msg::dh_key_exchange(session_, 2);

    if ((ret = dhpart->parse_msg(dh1_, session_, dh_len_)) != RTP_OK) {
        UVG_LOG_ERROR("Failed to parse DHPart1 Message!");
        return ret;
    }
    UVG_LOG_DEBUG("DHPart1 parsed");
    /* parse_msg() above extracted the public key of remote and saved it to session_.
     * Now we must generate shared secrets (DHResult, total_hash, and s0) */
    generate_shared_secrets_dh();

    uvgrtp::clock::hrc::hrc_t start = uvgrtp::clock::hrc::now();
    int interval = 150;
    int i = 1;

    while (true) {
        if (conf1_ != nullptr) {
            UVG_LOG_DEBUG("Confirm1 found");
            return RTP_OK;
        }

        long int next_sendslot = i * interval;
        long int run_time = (long int)uvgrtp::clock::hrc::diff_now(start);
        long int diff_ms = next_sendslot - run_time;

        if (diff_ms < 0) {
            if ((ret = dhpart->send_msg(local_socket_, remote_addr_, remote_ip6_addr_)) != RTP_OK) {
                UVG_LOG_ERROR("Failed to send DHPart2 Message!");
                return ret;
            }
            UVG_LOG_DEBUG("DHPart2 sent");
            if (interval < 1200) {
                interval *= 2;
            }
            ++i;
        }
        else {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        if (i > 10) {
            break;
        }
    }
    return RTP_TIMEOUT;
}

rtp_error_t uvgrtp::zrtp::responder_finalize_session()
{
    auto confirm = uvgrtp::zrtp_msg::confirm(session_, 1);
    auto confack = uvgrtp::zrtp_msg::confack(session_);
    uvgrtp::clock::hrc::hrc_t start = uvgrtp::clock::hrc::now();
    int interval = 150;
    int i = 1;

    while (true) {
        if (conf2_ != nullptr) {
            UVG_LOG_DEBUG("Confirm2 found");

            if (confirm.parse_msg(conf2_, session_) != RTP_OK) {
                UVG_LOG_ERROR("Failed to parse Confirm2 Message!");
                continue;
            }

            rtp_error_t ret = RTP_OK;
            if ((ret = validate_session()) != RTP_OK) {
                UVG_LOG_ERROR("Mismatch on one of the received MACs/Hashes, session cannot continue");
                return ret;
            }

            /* TODO: send in a loop? */
            confack.send_msg(local_socket_, remote_addr_, remote_ip6_addr_);
            UVG_LOG_DEBUG("ConfACK sent");

            return RTP_OK;
        }

        long int next_sendslot = i * interval;
        long int run_time = (long int)uvgrtp::clock::hrc::diff_now(start);
        long int diff_ms = next_sendslot - run_time;

        if (diff_ms < 0) {
            if (confirm.send_msg(local_socket_, remote_addr_, remote_ip6_addr_) != RTP_OK) {
                UVG_LOG_ERROR("Failed to send Confirm1 Message!");
            }
            ++i;
            UVG_LOG_DEBUG("Confirm1 sent");
            if (interval < 1200) {
                interval *= 2;
            }
        }
        else {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        if (i > 10) {
            break;
        }
    }
    return RTP_TIMEOUT;
}

rtp_error_t uvgrtp::zrtp::initiator_finalize_session()
{
    rtp_error_t ret = RTP_OK;
    auto confirm = uvgrtp::zrtp_msg::confirm(session_, 2);
    uvgrtp::clock::hrc::hrc_t start = uvgrtp::clock::hrc::now();
    int interval = 150;
    int i = 1;

    if ((ret = confirm.parse_msg(conf1_, session_)) != RTP_OK) {
        UVG_LOG_ERROR("Failed to parse Confirm1 Message!");
        return ret;
    }

    if ((ret = validate_session()) != RTP_OK) {
        UVG_LOG_ERROR("Mismatch on one of the received MACs/Hashes, session cannot continue");
        return ret;
    }

    while (true) {

        if (confack_ != nullptr) {
            UVG_LOG_DEBUG("ConfACK found");
            return RTP_OK;
        }

        long int next_sendslot = i * interval;
        long int run_time = (long int)uvgrtp::clock::hrc::diff_now(start);
        long int diff_ms = next_sendslot - run_time;

        if (diff_ms < 0) {
            if ((ret = confirm.send_msg(local_socket_, remote_addr_, remote_ip6_addr_)) != RTP_OK) {
                UVG_LOG_ERROR("Failed to send Confirm2 Message!");
                return ret;
            }
            UVG_LOG_DEBUG("ZRTP Confirm2 sent");
            ++i;
            if (interval < 1200) {
                interval *= 2;
            }
        }
        else {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        if (i > 10) {
            break;
        }
    }

    return RTP_TIMEOUT;
}

rtp_error_t uvgrtp::zrtp::init(uint32_t ssrc, std::shared_ptr<uvgrtp::socket> socket, sockaddr_in& addr, sockaddr_in6& addr6, bool perform_dh, bool ipv6)
{
    rtp_error_t ret = RTP_OK;

    if (perform_dh) 
    {
        zrtp_mtx_.lock();
        
        if (initialized_)
        {
            UVG_LOG_WARN("ZRTP multistream mode not used. Please use RCE_ZRTP_MULTISTREAM_MODE flag " \
                "to select which streams should not perform DH");
        }

        // perform Diffie-Hellman (DH)
        ret = init_dhm(ssrc, socket, addr, addr6, ipv6);
        zrtp_mtx_.unlock();
    }
    else
    {
        if (!initialized_)
        {
            UVG_LOG_ERROR("Attempted multistream mode for non initialized ZRTP");
            return RTP_GENERIC_ERROR;
        }

        // multistream mode
        ret = init_msm(ssrc, socket, addr, addr6);
    }

    return ret;
}

rtp_error_t uvgrtp::zrtp::init_dhm(uint32_t ssrc, std::shared_ptr<uvgrtp::socket> socket, sockaddr_in& addr, sockaddr_in6& addr6, bool ipv6)
{
    rtp_error_t ret = RTP_OK;
    if (ipv6) {
        UVG_LOG_DEBUG("Starting ZRTP Diffie-Hellman negotiation with %s", uvgrtp::socket::sockaddr_ip6_to_string(addr6).c_str());
    }
    else {
        UVG_LOG_DEBUG("Starting ZRTP Diffie-Hellman negotiation with %s", uvgrtp::socket::sockaddr_to_string(addr).c_str());
    }

    hello_ = nullptr;
    hello_ack_ = nullptr;
    commit_ = nullptr;
    dh1_ = nullptr;
    dh2_ = nullptr;
    conf1_ = nullptr;
    conf2_ = nullptr;
    confack_ = nullptr;

    /* TODO: set all fields initially to zero */
    memset(session_.hash_ctx.o_hvi, 0, sizeof(session_.hash_ctx.o_hvi));

    /* Generate ZID and random data for the retained secrets */
    generate_zid();
    generate_secrets();

    /* Initialize the session hashes H0 - H3 defined in Section 9 of RFC 6189 */
    init_session_hashes();

    local_socket_ = socket;
    remote_addr_ = addr;
    remote_ip6_addr_ = addr6;

    session_.seq  = 0;
    session_.ssrc = ssrc;

    /* Begin session by exchanging Hello and HelloACK messages.
     *
     * After begin_session() we know what remote is capable of
     * and whether we are compatible implementations */
    if ((ret = begin_session()) != RTP_OK) {
        UVG_LOG_ERROR("Session initialization failed, ZRTP cannot be used!");
        return ret;
    }

    /* After begin_session() we have remote's Hello message and we can craft
     * DHPart2 in the hopes that we're the Initiator.
     *
     * If this assumption proves to be false, we just discard the message
     * and create DHPart1.
     *
     * Commit message contains hash value of initiator (hvi) which is the
     * the hashed value of Initiators DHPart2 message and Responder's Hello
     * message. This should be calculated now because the next step is choosing
     * the the roles for participants. */
    auto dh_msg = uvgrtp::zrtp_msg::dh_key_exchange(session_, 2);
    cctx_.sha256->update((uint8_t *)session_.l_msg.dh.second,    session_.l_msg.dh.first);
    cctx_.sha256->update((uint8_t *)session_.r_msg.hello.second, session_.r_msg.hello.first);
    cctx_.sha256->final((uint8_t *)session_.hash_ctx.o_hvi);

    /* We're here which means that remote respond to us and sent a Hello message
     * with same version number as ours. This means that the implementations are
     * compatible with each other and we can start the actual negotiation
     *
     * Both participants create Commit messages which include the used algorithms
     * etc. used during the session + some extra information such as ZID
     *
     * init_session() will exchange the Commit messages and select roles for the
     * participants (initiator/responder) based on rules determined in RFC 6189 */
    if ((ret = init_session(DH3k)) != RTP_OK) {
        UVG_LOG_ERROR("Could not agree on ZRTP session parameters or roles of participants!");
        return ret;
    }

    /* From this point on, the execution deviates because both parties have their own roles
     * and different message that they need to send in order to finalize the ZRTP connection */
    if (session_.role == INITIATOR) {
        if ((ret = dh_part2(&dh_msg)) != RTP_OK) {
            UVG_LOG_ERROR("Failed to perform Diffie-Hellman key exchange Part2");
            return ret;
        }

        if ((ret = initiator_finalize_session()) != RTP_OK) {
            UVG_LOG_ERROR("Failed to finalize session using Confirm2");
            return ret;
        }

    } else {
        if ((ret = dh_part1()) != RTP_OK) {
            UVG_LOG_ERROR("Failed to perform Diffie-Hellman key exchange Part1");
            return ret;
        }

        if ((ret = responder_finalize_session()) != RTP_OK) {
            UVG_LOG_ERROR("Failed to finalize session using Confirm1/Conf2ACK");
            return ret;
        }
    }
    UVG_LOG_INFO("ZRTP has been initialized using DHMode");
    /* ZRTP has been initialized using DHMode */
    initialized_ = true;
    /* Session has been initialized successfully and SRTP can start */
    return RTP_OK;
}

rtp_error_t uvgrtp::zrtp::init_msm(uint32_t ssrc, std::shared_ptr<uvgrtp::socket> socket, sockaddr_in& addr, sockaddr_in6& addr6)
{
    rtp_error_t ret;

    local_socket_ = socket;
    remote_addr_ = addr;
    remote_ip6_addr_ = addr6;

    session_.ssrc = ssrc;
    session_.seq  = 0;

    hello_ = nullptr;
    hello_ack_ = nullptr;
    commit_ = nullptr;
    dh1_ = nullptr;
    dh2_ = nullptr;
    conf1_ = nullptr;
    conf2_ = nullptr;
    confack_ = nullptr;

    UVG_LOG_DEBUG("Generating ZRTP keys in multistream mode");

    if ((ret = begin_session()) != RTP_OK) {
        UVG_LOG_ERROR("Session initialization failed, ZRTP cannot be used!");
        return ret;
    }

    if ((ret = init_session(MULT)) != RTP_OK) {
        UVG_LOG_ERROR("Could not agree on ZRTP session parameters or roles of participants!");
        return ret;
    }

    if (session_.role == INITIATOR) {
        generate_shared_secrets_msm();

        if ((ret = initiator_finalize_session()) != RTP_OK) {
            UVG_LOG_ERROR("Failed to finalize session using Confirm2");
            return ret;
        }
    } else {
        generate_shared_secrets_msm();

        if ((ret = responder_finalize_session()) != RTP_OK) {
            UVG_LOG_ERROR("Failed to finalize session using Confirm1/Conf2ACK");
            return ret;
        }
    }
    return RTP_OK;
}

rtp_error_t uvgrtp::zrtp::get_srtp_keys(
    uint8_t *our_mkey,    uint32_t okey_len,
    uint8_t *their_mkey,  uint32_t tkey_len,
    uint8_t *our_msalt,   uint32_t osalt_len,
    uint8_t *their_msalt, uint32_t tsalt_len
)
{
    if (!our_mkey || !their_mkey || !our_msalt || !their_msalt ||
        !okey_len || !tkey_len   || !osalt_len || !tsalt_len)
    {
        return RTP_INVALID_VALUE;
    }

    if (!initialized_)
        return RTP_NOT_INITIALIZED;

    if (session_.role == INITIATOR) {
        derive_key("Initiator SRTP master key",  okey_len,  our_mkey);
        derive_key("Initiator SRTP master salt", osalt_len, our_msalt);

        derive_key("Responder SRTP master key",  tkey_len,  their_mkey);
        derive_key("Responder SRTP master salt", tsalt_len, their_msalt);
    } else {
        derive_key("Responder SRTP master key",  okey_len,  our_mkey);
        derive_key("Responder SRTP master salt", tsalt_len, our_msalt);

        derive_key("Initiator SRTP master key",  tkey_len,  their_mkey);
        derive_key("Initiator SRTP master salt", tsalt_len, their_msalt);
    }

    return RTP_OK;
}

rtp_error_t uvgrtp::zrtp::packet_handler(void* args, int rce_flags, uint8_t* read_ptr, size_t size, frame::rtp_frame** out)
{
    if (size < 0 || (uint32_t)size < sizeof(uvgrtp::zrtp_msg::zrtp_msg))
    {
        return RTP_PKT_NOT_HANDLED;
    }

    (void)args;
    (void)rce_flags;
    (void)out;

    auto msg = (uvgrtp::zrtp_msg::zrtp_msg*)read_ptr;

    /* not a ZRTP packet */
    if (msg->header.version ||  msg->preamble != ZRTP_PREAMBLE) {
        return RTP_PKT_NOT_HANDLED;
    }
    switch (msg->msgblock) {
        case ZRTP_MSG_HELLO:
        {
            // TODO: Check length based on algorithms
            if (msg->length < 22) // see rfc 6189 section 5.8
            {
                UVG_LOG_WARN("ZRTP Hello length field is wrong");
                return RTP_INVALID_VALUE;
            }

            //UVG_LOG_DEBUG("ZRTP Hello message received, verify CRC32!");
            zrtp_hello* hello = (zrtp_hello*)msg;

            if (!uvgrtp::crypto::crc32::verify_crc32(read_ptr, size - 4, hello->crc)) {
                return RTP_NOT_SUPPORTED;
            }
            if (hello_ != nullptr) {
                //UVG_LOG_DEBUG("Already got Hello, discarding new one");
                return RTP_OK;
            }
            hello_ = hello;
            hello_len_ = size;
            return RTP_OK;
        }


        case ZRTP_MSG_HELLO_ACK:
        {
            if (msg->length != 3) // see rfc 6189 section 5.3
            {
                UVG_LOG_WARN("ZRTP Hello ACK length field is wrong");
                return RTP_INVALID_VALUE;
            }

            //UVG_LOG_DEBUG("ZRTP HelloACK message received, verify CRC32!");

            zrtp_hello_ack* ha_msg = (zrtp_hello_ack*)msg;

            if (!uvgrtp::crypto::crc32::verify_crc32(read_ptr, size - 4, ha_msg->crc))
                return RTP_NOT_SUPPORTED;
            if (hello_ack_ != nullptr) {
                //UVG_LOG_DEBUG("Already got HelloACK, discarding new one");
                return RTP_OK;
            }
            hello_ack_ = ha_msg;
            return RTP_OK;
        }

        case ZRTP_MSG_COMMIT:
        {
            if (msg->length != 29 &&
                msg->length != 25 &&
                msg->length != 27) // see rfc 6189 section 5.4
            {
                UVG_LOG_WARN("ZRTP Commit length field is wrong");
                return RTP_INVALID_VALUE;
            }

            //UVG_LOG_DEBUG("ZRTP Commit message received, verify CRC32!");

            zrtp_commit* commit = (zrtp_commit*)msg;

            if (!uvgrtp::crypto::crc32::verify_crc32(read_ptr, size - 4, commit->crc))
                return RTP_NOT_SUPPORTED;

            if (commit_ != nullptr) {
                UVG_LOG_DEBUG("Already got Commit, discarding new one");
                return RTP_OK;
            }
            commit_ = commit;
            commit_len_ = size;
            return RTP_OK;
        }

        case ZRTP_MSG_DH_PART1:
        {
            // TODO: Check based on KA type
            if (msg->length < 21) // see rfc 6189 section 5.5
            {
                UVG_LOG_WARN("ZRTP DH Part1 length field is wrong");
                return RTP_INVALID_VALUE;
            }

            //UVG_LOG_DEBUG("ZRTP DH Part1 message received, verify CRC32!");

            zrtp_dh* dh = (zrtp_dh*)msg;

            if (!uvgrtp::crypto::crc32::verify_crc32(read_ptr, size - 4, dh->crc))
                return RTP_NOT_SUPPORTED;

            if (dh1_ != nullptr) {
                UVG_LOG_DEBUG("Already got DHPart1, discarding new one");
                return RTP_OK;
            }
            dh1_ = dh;
            dh_len_ = size;
            return RTP_OK;
        }

        case ZRTP_MSG_DH_PART2:
        {
            // TODO: Check based on KA type
            if (msg->length < 21) // see rfc 6189 section 5.6
            {
                UVG_LOG_WARN("ZRTP DH Part2 length field is wrong");
                return RTP_INVALID_VALUE;
            }

            //UVG_LOG_DEBUG("ZRTP DH Part2 message received, verify CRC32!");

            zrtp_dh* dh = (zrtp_dh*)msg;

            if (!uvgrtp::crypto::crc32::verify_crc32(read_ptr, size - 4, dh->crc))
                return RTP_NOT_SUPPORTED;

            if (dh2_ != nullptr) {
                UVG_LOG_DEBUG("Already got DHPart2, discarding new one");
                return RTP_OK;
            }
            dh2_ = dh;
            dh_len_ = size;
            return RTP_OK;
        }

        case ZRTP_MSG_CONFIRM1:
        {
            // TODO: Check based on signiture
            if (msg->length < 19) // see rfc 6189 section 5.6
            {
                UVG_LOG_WARN("ZRTP Confirm1 length field is wrong");
                return RTP_INVALID_VALUE;
            }
            //UVG_LOG_DEBUG("ZRTP Confirm1 message received, verify CRC32!");

            zrtp_confirm* dh = (zrtp_confirm*)msg;

            if (!uvgrtp::crypto::crc32::verify_crc32(read_ptr, size - 4, dh->crc))
                return RTP_NOT_SUPPORTED;

            if (conf1_ != nullptr) {
                UVG_LOG_DEBUG("Already got Confirm1, discarding new one");
                return RTP_OK;
            }
            conf1_ = dh;
            return RTP_OK;
        }

        case ZRTP_MSG_CONFIRM2:
        {
            // TODO: Check based on signiture
            if (msg->length < 19) // see rfc 6189 section 5.6
            {
                UVG_LOG_WARN("ZRTP Confirm1 length field is wrong");
                return RTP_INVALID_VALUE;
            }

            //UVG_LOG_DEBUG("ZRTP Confirm2 message received, verify CRC32!");

            zrtp_confirm* dh = (zrtp_confirm*)msg;

            if (!uvgrtp::crypto::crc32::verify_crc32(read_ptr, size - 4, dh->crc))
                return RTP_NOT_SUPPORTED;

            if (conf2_ != nullptr) {
                UVG_LOG_DEBUG("Already got Confirm2, discarding new one");
                return RTP_OK;
            }
            conf2_ = dh;
            return RTP_OK;
        }

        case ZRTP_MSG_CONF2_ACK:
        {
            if (msg->length != 3) // see rfc 6189 section 5.8
            {
                UVG_LOG_WARN("ZRTP Conf2 ACK length field is wrong");
                return RTP_INVALID_VALUE;
            }

            //UVG_LOG_DEBUG("ZRTP Conf2 ACK message received, verify CRC32!");

            zrtp_confack* ca = (zrtp_confack*)msg;

            if (!uvgrtp::crypto::crc32::verify_crc32(read_ptr, size - 4, ca->crc))
                return RTP_NOT_SUPPORTED;

            if (confack_ != nullptr) {
                UVG_LOG_DEBUG("Already got Conf2ACK, discarding new one");
                return RTP_OK;
            }
            confack_ = ca;
            return RTP_OK;
        }

        case ZRTP_MSG_ERROR:
        {
            if (msg->length != 4) // see rfc 6189 section 5.9
            {
                UVG_LOG_WARN("ZRTP Error length field is wrong");
                return RTP_INVALID_VALUE;
            }

            UVG_LOG_DEBUG("ZRTP Error message received");
            return RTP_OK;
        }

        case ZRTP_MSG_ERROR_ACK:
        {
            if (msg->length != 3) // see rfc 6189 section 5.10
            {
                UVG_LOG_WARN("ZRTP Error ACK length field is wrong");
                return RTP_INVALID_VALUE;
            }

            UVG_LOG_DEBUG("ZRTP Error ACK message received");
            return RTP_OK;
        }

        case ZRTP_MSG_SAS_RELAY:
        {
            // TODO: Check based on signiture
            if (msg->length < 19) // see rfc 6189 section 5.14
            {
                UVG_LOG_WARN("ZRTP SAS Relay length field is wrong");
                return RTP_INVALID_VALUE;
            }

            UVG_LOG_DEBUG("ZRTP SAS Relay message received");
            return RTP_OK;
        }

        case ZRTP_MSG_RELAY_ACK:
        {
            if (msg->length != 3) // see rfc 6189 section 5.14
            {
                UVG_LOG_WARN("ZRTP Relay ACK length field is wrong");
                return RTP_INVALID_VALUE;
            }

            UVG_LOG_DEBUG("ZRTP Relay ACK message received");
            return RTP_OK;
        }

        case ZRTP_MSG_PING_ACK:
        {
            if (msg->length != 9) // see rfc 6189 section 5.16
            {
                UVG_LOG_WARN("ZRTP Relay ACK length field is wrong");
                return RTP_INVALID_VALUE;
            }

            UVG_LOG_DEBUG("ZRTP Ping ACK message received");
            return RTP_OK;
        }

        default: {
            return RTP_OK;
        }
    }
}