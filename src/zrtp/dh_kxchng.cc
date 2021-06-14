#include "dh_kxchng.hh"

#include "zrtp_receiver.hh"

#include "../zrtp.hh"
#include "crypto.hh"
#include "frame.hh"
#include "socket.hh"
#include "debug.hh"

#include <cstring>

#define ZRTP_DH_PART1       "DHPart1 "
#define ZRTP_DH_PART2       "DHPart2 "

uvgrtp::zrtp_msg::dh_key_exchange::dh_key_exchange(zrtp_session_t& session, int part):
    zrtp_message()
{
    const char *strs[2][2] = {
        {
            ZRTP_DH_PART1, "Responder"
        },
        {
            ZRTP_DH_PART2, "Initiator"
        }
    };

    LOG_DEBUG("Create ZRTP DHPart%d message", part);

    allocate_frame(sizeof(zrtp_dh));
    zrtp_dh* msg = (zrtp_dh*)frame_;
    set_zrtp_start(msg->msg_start, session, strs[part - 1][0]);

    msg->crc = 0;
    memcpy(msg->hash,                session.hash_ctx.o_hash[1], 32);

    /* Calculate hashes for the secrets (as defined in Section 4.3.1)
     *
     * These hashes are truncated to 64 bits so we use one temporary
     * buffer to store the full digest from which we copy the truncated
     * hash directly to the DHPartN message */
    uint8_t mac_full[32];

    /* rs1IDr */
    auto hmac_sha256 = uvgrtp::crypto::hmac::sha256(session.secrets.rs1, 32);
    hmac_sha256.update((uint8_t *)strs[part - 1][1], 9);
    hmac_sha256.final(mac_full);
    memcpy(msg->rs1_id, mac_full, 8);

    /* rs2IDr */
    hmac_sha256 = uvgrtp::crypto::hmac::sha256(session.secrets.rs2, 32);
    hmac_sha256.update((uint8_t *)strs[part - 1][1], 9);
    hmac_sha256.final(mac_full);
    memcpy(msg->rs2_id, mac_full, 8);

    /* auxsecretIDr */
    hmac_sha256 = uvgrtp::crypto::hmac::sha256(session.secrets.raux, 32);
    hmac_sha256.update(session.hash_ctx.o_hash[3], 32);
    hmac_sha256.final(mac_full);
    memcpy(msg->aux_secret, mac_full, 8);

    /* pbxsecretIDr */
    hmac_sha256 = uvgrtp::crypto::hmac::sha256(session.secrets.rpbx, 32);
    hmac_sha256.update((uint8_t *)strs[part - 1][1], 9);
    hmac_sha256.final(mac_full);
    memcpy(msg->pbx_secret, mac_full, 8);

    /* public key */
    memcpy(msg->pk, session.dh_ctx.public_key, sizeof(session.dh_ctx.public_key));

    /* Calculate truncated HMAC-SHA256 for the Commit Message */
    hmac_sha256 = uvgrtp::crypto::hmac::sha256(session.hash_ctx.o_hash[0], 32);
    hmac_sha256.update((uint8_t *)frame_, len_ - 8 - 4);
    hmac_sha256.final(mac_full);

    memcpy(msg->mac, mac_full, 8);

    /* Calculate CRC32 for the whole ZRTP packet */
    msg->crc = uvgrtp::crypto::crc32::calculate_crc32((uint8_t *)frame_, len_ - sizeof(uint32_t));

    /* Finally make a copy of the message and save it for later use */
    if (session.l_msg.dh.second)
        delete[] session.l_msg.dh.second ;

    session.l_msg.dh.first  = len_;
    session.l_msg.dh.second = (uvgrtp::zrtp_msg::zrtp_dh *)new uint8_t[len_];
    memcpy(session.l_msg.dh.second, msg, len_);
}

uvgrtp::zrtp_msg::dh_key_exchange::dh_key_exchange(struct zrtp_dh *dh):
    zrtp_message()
{
    (void)dh;
}

uvgrtp::zrtp_msg::dh_key_exchange::~dh_key_exchange()
{}

rtp_error_t uvgrtp::zrtp_msg::dh_key_exchange::parse_msg(uvgrtp::zrtp_msg::receiver& receiver, zrtp_session_t& session)
{
    LOG_DEBUG("Parsing DHPart1/DHPart2 message...");

    ssize_t len = 0;
    allocate_rframe(sizeof(zrtp_dh));
    if ((len = receiver.get_msg(rframe_, rlen_)) < 0) {
        LOG_ERROR("Failed to get message from ZRTP receiver");
        return RTP_INVALID_VALUE;
    }

    zrtp_dh *msg = (zrtp_dh *)rframe_;

    memcpy(session.dh_ctx.remote_public, msg->pk, 384);

    /* Because uvgRTP only supports DH mode, the retained secrets sent in this
     * DHPartN message are not going to match our own so there not point in parsing them.
     *
     * TODO support maybe preshared at some point? */
    session.secrets.s1 = nullptr;
    session.secrets.s2 = nullptr;
    session.secrets.s3 = nullptr;

    /* Save the MAC value so we can check if later */
    memcpy(&session.hash_ctx.r_mac[1],  &msg->mac,  8);
    memcpy(&session.hash_ctx.r_hash[1], msg->hash, 32);

    /* Finally make a copy of the message and save it for later use */
    session.r_msg.dh.first  = len;
    session.r_msg.dh.second = (uvgrtp::zrtp_msg::zrtp_dh *)new uint8_t[len];
    memcpy(session.r_msg.dh.second, msg, len);

    return RTP_OK;
}
