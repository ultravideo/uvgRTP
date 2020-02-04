#include <cstring>

#include "debug.hh"
#include "zrtp.hh"
#include "mzrtp/dh_kxchng.hh"
#include "mzrtp/defines.hh"

#define ZRTP_DH_PART1       "DHPart1 "
#define ZRTP_DH_PART2       "DHPart2 "

kvz_rtp::zrtp_msg::dh_key_exchange::dh_key_exchange(zrtp_session_t& session, int part)
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

    frame_  = kvz_rtp::frame::alloc_zrtp_frame(sizeof(zrtp_dh));
    rframe_ = kvz_rtp::frame::alloc_zrtp_frame(sizeof(zrtp_dh));
    len_    = sizeof(zrtp_dh);
    rlen_   = sizeof(zrtp_dh);

    memset(frame_,  0, sizeof(zrtp_dh));
    memset(rframe_, 0, sizeof(zrtp_dh));

    zrtp_dh *msg = (zrtp_dh *)frame_;

    msg->msg_start.header.version = 0;
    msg->msg_start.header.unused  = 0;
    msg->msg_start.header.magic   = ZRTP_HEADER_MAGIC;
    msg->msg_start.header.ssrc    = session.ssrc;
    msg->msg_start.header.seq     = session.seq++;

    msg->msg_start.magic  = ZRTP_MSG_MAGIC;
    msg->msg_start.length = len_ - sizeof(zrtp_header);
    msg->crc = 0;

    memcpy(&msg->msg_start.msgblock, strs[part - 1][0],           8);
    memcpy(msg->hash,                session.hash_ctx.o_hash[1], 32);

    /* Calculate hashes for the secrets (as defined in Section 4.3.1)
     *
     * These hashes are truncated to 64 bits so we use one temporary
     * buffer to store the full digest from which we copy the truncated
     * hash directly to the DHPartN message */
    uint8_t mac_full[32];

    /* rs1IDr */
    auto hmac_sha256 = kvz_rtp::crypto::hmac::sha256(session.secrets.rs1, 32);
    hmac_sha256.update((uint8_t *)strs[part - 1][1], 9);
    hmac_sha256.final(mac_full);
    memcpy(msg->rs1_id, mac_full, 8);

    /* rs2IDr */
    hmac_sha256 = kvz_rtp::crypto::hmac::sha256(session.secrets.rs2, 32);
    hmac_sha256.update((uint8_t *)strs[part - 1][1], 9);
    hmac_sha256.final(mac_full);
    memcpy(msg->rs2_id, mac_full, 8);

    /* auxsecretIDr */
    hmac_sha256 = kvz_rtp::crypto::hmac::sha256(session.secrets.raux, 32);
    hmac_sha256.update(session.hash_ctx.o_hash[3], 32);
    hmac_sha256.final(mac_full);
    memcpy(msg->aux_secret, mac_full, 8);

    /* pbxsecretIDr */
    hmac_sha256 = kvz_rtp::crypto::hmac::sha256(session.secrets.rpbx, 32);
    hmac_sha256.update((uint8_t *)strs[part - 1][1], 9);
    hmac_sha256.final(mac_full);
    memcpy(msg->pbx_secret, mac_full, 8);

    /* public key */
    memcpy(msg->pk, session.dh_ctx.public_key, sizeof(session.dh_ctx.public_key));

    /* Calculate truncated HMAC-SHA256 for the Commit Message */
    hmac_sha256 = kvz_rtp::crypto::hmac::sha256(session.hash_ctx.o_hash[0], 32);
    hmac_sha256.update((uint8_t *)frame_, len_ - 8 - 4);
    hmac_sha256.final(mac_full);

    memcpy(msg->mac, mac_full, 8);

    /* Calculate CRC32 for the whole ZRTP packet */
    kvz_rtp::crypto::crc32::get_crc32((uint8_t *)frame_, len_ - 4, &msg->crc);

    /* Finally make a copy of the message and save it for later use */
    session.l_msg.dh.first  = len_;
    session.l_msg.dh.second = (kvz_rtp::zrtp_msg::zrtp_dh *)new uint8_t[len_];
    memcpy(session.l_msg.dh.second, msg, len_);
}

kvz_rtp::zrtp_msg::dh_key_exchange::dh_key_exchange(struct zrtp_dh *dh)
{
    (void)dh;
}

kvz_rtp::zrtp_msg::dh_key_exchange::~dh_key_exchange()
{
    LOG_DEBUG("Freeing DHPartN message...");

    (void)kvz_rtp::frame::dealloc_frame(frame_);
    (void)kvz_rtp::frame::dealloc_frame(rframe_);
}

rtp_error_t kvz_rtp::zrtp_msg::dh_key_exchange::send_msg(socket_t& socket, sockaddr_in& addr)
{
#ifdef __linux
    if (::sendto(socket, (void *)frame_, len_, 0, (const struct sockaddr *)&addr, (socklen_t)sizeof(addr)) < 0) {
        LOG_ERROR("Failed to send ZRTP Hello message: %s!", strerror(errno));
        return RTP_SEND_ERROR;
    }
#else
    DWORD sent_bytes;
    WSABUF data_buf;

    data_buf.buf = (char *)frame_;
    data_buf.len = len_;

    if (WSASendTo(socket, &data_buf, 1, nullptr, 0, (const struct sockaddr *)&addr, sizeof(addr), nullptr, nullptr) == -1) {
        win_get_last_error();

        return RTP_SEND_ERROR;
    }
#endif

    return RTP_OK;
}

rtp_error_t kvz_rtp::zrtp_msg::dh_key_exchange::parse_msg(kvz_rtp::zrtp_msg::receiver& receiver, zrtp_session_t& session)
{
    LOG_DEBUG("Parsing DHPart1/DHPart2 message...");

    ssize_t len = 0;

    if ((len = receiver.get_msg(rframe_, rlen_)) < 0) {
        LOG_ERROR("Failed to get message from ZRTP receiver");
        return RTP_INVALID_VALUE;
    }

    zrtp_dh *msg = (zrtp_dh *)rframe_;

    memcpy(session.dh_ctx.remote_public, msg->pk, 384);

    /* Because kvzRTP only supports DH mode, the retained secrets sent in this
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
    session.r_msg.dh.second = (kvz_rtp::zrtp_msg::zrtp_dh *)new uint8_t[len];
    memcpy(session.r_msg.dh.second, msg, len);

    return RTP_OK;
}
