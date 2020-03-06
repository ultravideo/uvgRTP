#ifdef __RTP_CRYPTO__
#include <cstring>

#include "debug.hh"
#include "zrtp.hh"

#include "mzrtp/confirm.hh"

#define ZRTP_CONFRIM1 "Confirm1"
#define ZRTP_CONFRIM2 "Confirm2"

kvz_rtp::zrtp_msg::confirm::confirm(zrtp_session_t& session, int part)
{
    /* temporary storage for the full hmac hash */
    uint8_t mac_full[32];

    LOG_DEBUG("Create ZRTP Confirm%d message!", part);

    frame_  = kvz_rtp::frame::alloc_zrtp_frame(sizeof(zrtp_confirm));
    rframe_ = kvz_rtp::frame::alloc_zrtp_frame(sizeof(zrtp_confirm));

    len_    = sizeof(zrtp_confirm);
    rlen_   = sizeof(zrtp_confirm);

    memset(frame_,  0, sizeof(zrtp_confirm));
    memset(rframe_, 0, sizeof(zrtp_confirm));

    zrtp_confirm *msg = (zrtp_confirm *)frame_;

    msg->msg_start.header.version = 0;
    msg->msg_start.header.magic   = ZRTP_HEADER_MAGIC;
    msg->msg_start.header.ssrc    = session.ssrc;
    msg->msg_start.header.seq     = session.seq++;

    msg->msg_start.magic  = ZRTP_MSG_MAGIC;
    msg->msg_start.length = len_ - sizeof(zrtp_header);

    /* Message Type Block and H0 */
    memcpy(&msg->msg_start.msgblock, (part == 1) ? ZRTP_CONFRIM1 : ZRTP_CONFRIM2,  8);
    memcpy(&msg->hash,               session.hash_ctx.o_hash[0],                  32); /* 256 bits */

    /* Generate random 128-bit nonce for CFB IV */
    kvz_rtp::crypto::random::generate_random(msg->cfb_iv, 16);

    kvz_rtp::crypto::hmac::sha256 *hmac_sha256;
    kvz_rtp::crypto::aes::cfb     *aes_cfb;

    /* Responder */
    if (part == 1) {
        aes_cfb     = new kvz_rtp::crypto::aes::cfb(session.key_ctx.zrtp_keyr, 16, msg->cfb_iv);
        hmac_sha256 = new kvz_rtp::crypto::hmac::sha256(session.key_ctx.hmac_keyr, 32);

    /* Initiator */
    } else {
        aes_cfb     = new kvz_rtp::crypto::aes::cfb(session.key_ctx.zrtp_keyi, 16, msg->cfb_iv);
        hmac_sha256 = new kvz_rtp::crypto::hmac::sha256(session.key_ctx.hmac_keyi, 32);
    }

    msg->e          = 0;
    msg->v          = 0;
    msg->d          = 0;
    msg->a          = 0;
    msg->unused     = 0;
    msg->zeros      = 0;
    msg->sig_len    = 0;
    msg->cache_expr = 0;

    aes_cfb->encrypt((uint8_t *)msg->hash, (uint8_t *)msg->hash, 40);

    /* Calculate HMAC-SHA256 of the encrypted portion of the message */
    hmac_sha256->update((uint8_t *)msg->hash, 40);
    hmac_sha256->final(mac_full);
    memcpy(&msg->confirm_mac, mac_full, sizeof(uint64_t));

    /* Calculate CRC32 for the whole ZRTP packet */
    kvz_rtp::crypto::crc32::get_crc32((uint8_t *)frame_, len_ - 4, &msg->crc);

    delete hmac_sha256;
    delete aes_cfb;
}

kvz_rtp::zrtp_msg::confirm::~confirm()
{
    LOG_DEBUG("Freeing ConfirmN message...");

    (void)kvz_rtp::frame::dealloc_frame(frame_);
    (void)kvz_rtp::frame::dealloc_frame(rframe_);
}

rtp_error_t kvz_rtp::zrtp_msg::confirm::send_msg(socket_t& socket, sockaddr_in& addr)
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

    if (WSASendTo(socket, &data_buf, 1, NULL, 0, (const struct sockaddr *)&addr, sizeof(addr), nullptr, nullptr) == -1) {
        win_get_last_error();

        return RTP_SEND_ERROR;
    }
#endif

    return RTP_OK;
}

rtp_error_t kvz_rtp::zrtp_msg::confirm::parse_msg(kvz_rtp::zrtp_msg::receiver& receiver, zrtp_session_t& session)
{
    ssize_t len          = 0;
    uint64_t mac         = 0;
    uint64_t cmac        = 0;
    uint8_t mac_full[32] = { 0 };

    if ((len = receiver.get_msg(rframe_, rlen_)) < 0) {
        LOG_ERROR("Failed to get message from ZRTP receiver");
        return RTP_INVALID_VALUE;
    }

    kvz_rtp::crypto::hmac::sha256 *hmac_sha256 = nullptr;
    kvz_rtp::crypto::aes::cfb *aes_cfb         = nullptr;
    zrtp_confirm *msg                          = (zrtp_confirm *)rframe_;

    if (!memcmp(&msg->msg_start.msgblock, (const void *)ZRTP_CONFRIM1, sizeof(uint64_t))) {
        aes_cfb     = new kvz_rtp::crypto::aes::cfb(session.key_ctx.zrtp_keyr, 16, msg->cfb_iv);
        hmac_sha256 = new kvz_rtp::crypto::hmac::sha256(session.key_ctx.hmac_keyr, 32);
    } else {
        aes_cfb     = new kvz_rtp::crypto::aes::cfb(session.key_ctx.zrtp_keyi, 16, msg->cfb_iv);
        hmac_sha256 = new kvz_rtp::crypto::hmac::sha256(session.key_ctx.hmac_keyi, 32);
    }

    /* Verify confirm_mac before decrypting the message */
    hmac_sha256->update((uint8_t *)msg->hash, 40);
    hmac_sha256->final(mac_full);

    memcpy(&mac,  mac_full,         sizeof(uint64_t));
    memcpy(&cmac, msg->confirm_mac, sizeof(uint64_t));

    if (mac != cmac)
        return RTP_INVALID_VALUE;

    aes_cfb->decrypt((uint8_t *)msg->hash, (uint8_t *)msg->hash, 40);

    /* Finally save the first hash H0 so we can verify other MAC values received.
     * The first (last) remote mac is not used */
    memcpy(&session.hash_ctx.r_hash[0], &msg->hash, 32);
    session.hash_ctx.r_mac[0] = 0;

    delete aes_cfb;
    delete hmac_sha256;

    return RTP_OK;
}
#endif
