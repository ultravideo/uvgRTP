#include "commit.hh"

#include "../zrtp.hh"
#include "crypto.hh"
#include "debug.hh"
#include "frame.hh"
#include "socket.hh"

#include <cassert>
#include <cstring>

#define ZRTP_COMMIT "Commit  "

uvgrtp::zrtp_msg::commit::commit(zrtp_session_t& session):
    zrtp_message()
{
    /* temporary storage for the full hmac hash */
    uint8_t mac_full[32] = { 0 };

    LOG_DEBUG("Create ZRTP Commit message!");

    allocate_frame(sizeof(zrtp_commit));
    zrtp_commit *msg = (zrtp_commit *)frame_;
    set_zrtp_start(msg->msg_start, session, ZRTP_COMMIT);

    memcpy(msg->zid,                 session.o_zid,              12); /* 96 bits */
    memcpy(msg->hash,                session.hash_ctx.o_hash[2], 32); /* 256 bits */

    /* Multistream mode must use unique random nonce */
    if (session.key_agreement_type == MULT) {
        memset((uint8_t *)session.hash_ctx.o_hvi, 0, 32);
        uvgrtp::crypto::random::generate_random((uint8_t *)session.hash_ctx.o_hvi, 16);
        memcpy(msg->hvi, session.hash_ctx.o_hvi, 16); /* 128 bits */
    } else {
        memcpy(msg->hvi, session.hash_ctx.o_hvi, 32); /* 256 bits */
    }

    msg->sas_type           = session.sas_type;
    msg->hash_algo          = session.hash_algo;
    msg->cipher_algo        = session.cipher_algo;
    msg->auth_tag_type      = session.auth_tag_type;
    msg->key_agreement_type = session.key_agreement_type;

    /* Calculate truncated HMAC-SHA256 for the Commit Message */
    auto hmac_sha256 = uvgrtp::crypto::hmac::sha256(session.hash_ctx.o_hash[1], 32);

    hmac_sha256.update((uint8_t *)frame_, len_ - 8 - 4);
    hmac_sha256.final(mac_full);

    memcpy(&msg->mac, mac_full, sizeof(uint64_t));

    /* Calculate CRC32 for the whole ZRTP packet */
    msg->crc = uvgrtp::crypto::crc32::calculate_crc32((uint8_t *)frame_, len_ - sizeof(uint32_t));

    /* Finally make a copy of the message and save it for later use */
    session.l_msg.commit.first  = len_;
    session.l_msg.commit.second = (uvgrtp::zrtp_msg::zrtp_commit *)new uint8_t[len_];
    memcpy(session.l_msg.commit.second, msg, len_);
}

uvgrtp::zrtp_msg::commit::~commit()
{}


rtp_error_t uvgrtp::zrtp_msg::commit::parse_msg(uvgrtp::zrtp_msg::receiver& receiver, zrtp_session_t& session)
{
    ssize_t len = 0;
    allocate_rframe(sizeof(zrtp_commit));

    if ((len = receiver.get_msg(rframe_, rlen_)) < 0) {
        LOG_ERROR("Failed to get message from ZRTP receiver");
        return RTP_INVALID_VALUE;
    }

    zrtp_commit *msg = (zrtp_commit *)rframe_;

    session.sas_type           = msg->sas_type;
    session.hash_algo          = msg->hash_algo;
    session.cipher_algo        = msg->cipher_algo;
    session.auth_tag_type      = msg->auth_tag_type;
    session.key_agreement_type = msg->key_agreement_type;

    if (session.key_agreement_type == MULT)
        memcpy(session.hash_ctx.r_hvi, msg->hvi, 16);
    else
        memcpy(session.hash_ctx.r_hvi, msg->hvi, 32);

    memcpy(&session.hash_ctx.r_mac[2], &msg->mac,  8);
    memcpy(session.hash_ctx.r_hash[2], msg->hash, 32);

    /* Finally make a copy of the message and save it for later use */
    session.r_msg.commit.first  = len;
    session.r_msg.commit.second = (uvgrtp::zrtp_msg::zrtp_commit *)new uint8_t[len];
    memcpy(session.r_msg.commit.second, msg, len);

    return RTP_OK;
}
