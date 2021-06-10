#include "hello.hh"

#include "zrtp_receiver.hh"

#include "../zrtp.hh"
#include "crypto.hh"
#include "frame.hh"
#include "socket.hh"
#include "debug.hh"



#include <cstring>

#define ZRTP_VERSION     "1.10"
#define ZRTP_HELLO       "Hello   "
#define ZRTP_CLIENT_ID   "uvgRTP,UVG,TUNI "

using namespace uvgrtp::zrtp_msg;

uvgrtp::zrtp_msg::hello::hello(zrtp_session_t& session):
    zrtp_message()
{
    /* temporary storage for the full hmac hash */
    uint8_t mac_full[32];

    /* We support only the mandatory algorithms etc. defined in RFC 6189
     * so for us all the hash algos and key agreement types are set to zero 
     *
     * We need to assume that remote supports everything and thus need to
     * allocate the maximum amount of memory for the message  */

    allocate_frame(sizeof(zrtp_hello));

    zrtp_hello* msg = (zrtp_hello*)frame_;
    set_zrtp_start(msg->msg_start, session, ZRTP_HELLO);

    memcpy(&msg->version,            ZRTP_VERSION,                4);
    memcpy(&msg->client,             ZRTP_CLIENT_ID,             16);
    memcpy(&msg->hash,               session.hash_ctx.o_hash[3], 32); /* 256 bits */
    memcpy(&msg->zid,                session.o_zid,              12); /* 96 bits */

    msg->zero   = 0;
    msg->s      = 0;
    msg->m      = 0;
    msg->p      = 0;
    msg->unused = 0;
    msg->hc     = 0;
    msg->ac     = 0;
    msg->kc     = 0;
    msg->sc     = 0;

    /* Calculate MAC for the Hello message (only the ZRTP message part) */
    auto hmac_sha256 = uvgrtp::crypto::hmac::sha256(session.hash_ctx.o_hash[2], 32);

    hmac_sha256.update((uint8_t *)frame_, 81);
    hmac_sha256.final(mac_full);

    memcpy(&msg->mac, mac_full, sizeof(uint64_t));

    /* Calculate CRC32 of the whole packet (excluding crc) */
    msg->crc = uvgrtp::crypto::crc32::calculate_crc32((uint8_t *)frame_, len_ - sizeof(uint32_t));

    /* Finally make a copy of the message and save it for later use */
    session.l_msg.hello.first  = len_;
    session.l_msg.hello.second = (uvgrtp::zrtp_msg::zrtp_hello *)new uint8_t[len_];
    memcpy(session.l_msg.hello.second, msg, len_);
}

uvgrtp::zrtp_msg::hello::~hello()
{}

rtp_error_t uvgrtp::zrtp_msg::hello::parse_msg(uvgrtp::zrtp_msg::receiver& receiver, zrtp_session_t& session)
{
    ssize_t len = 0;
    allocate_rframe(sizeof(zrtp_hello) + 5 * 8);
    if ((len = receiver.get_msg(rframe_, rlen_)) < 0) {
        LOG_ERROR("Failed to get message from ZRTP receiver");
        return RTP_INVALID_VALUE;
    }

    zrtp_hello *msg = (zrtp_hello *)rframe_;

    if (strncmp((const char *)&msg->version, ZRTP_VERSION, 4)) {
        LOG_ERROR("Invalid ZRTP version!");
        session.capabilities.version = 0;
    } else {
        session.capabilities.version = 110;
    }

    /* finally add mandatory algorithms required by the specification to remote capabilities */
    session.capabilities.hash_algos.push_back(S256);
    session.capabilities.cipher_algos.push_back(AES1);
    session.capabilities.auth_tags.push_back(HS32);
    session.capabilities.auth_tags.push_back(HS80);
    session.capabilities.key_agreements.push_back(DH3k);
    session.capabilities.sas_types.push_back(B32);

    /* Save the MAC value so we can check if later */
    memcpy(&session.hash_ctx.r_mac[3],  &msg->mac,  8);
    memcpy(&session.hash_ctx.r_hash[3], msg->hash, 32);

    /* Save ZID */
    memcpy(session.r_zid, msg->zid, 12);

    /* Finally make a copy of the message and save it for later use */
    session.r_msg.hello.first  = len;
    session.r_msg.hello.second = (uvgrtp::zrtp_msg::zrtp_hello *)new uint8_t[len];
    memcpy(session.r_msg.hello.second, msg, len);

    return RTP_OK;
}
