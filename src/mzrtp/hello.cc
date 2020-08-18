#ifdef __RTP_CRYPTO__
#include <cstring>

#include "debug.hh"
#include "zrtp.hh"
#include "mzrtp/hello.hh"
#include "mzrtp/defines.hh"

#define ZRTP_VERSION     "1.10"
#define ZRTP_HELLO       "Hello   "
#define ZRTP_CLIENT_ID   "uvgRTP,UVG,TUNI "

using namespace uvg_rtp::zrtp_msg;

uvg_rtp::zrtp_msg::hello::hello(zrtp_session_t& session)
{
    /* temporary storage for the full hmac hash */
    uint8_t mac_full[32];

    /* We support only the mandatory algorithms etc. defined in RFC 6189
     * so for us all the hash algos and key agreement types are set to zero 
     *
     * We need to assume that remote supports everything and thus need to
     * allocate the maximum amount of memory for the message  */
    len_  = sizeof(zrtp_hello);
    rlen_ = sizeof(zrtp_hello) + 5 * 8;

    frame_  = uvg_rtp::frame::alloc_zrtp_frame(sizeof(zrtp_hello));
    rframe_ = uvg_rtp::frame::alloc_zrtp_frame(sizeof(zrtp_hello) + 5 * 8);

    memset(frame_,  0, sizeof(zrtp_hello));
    memset(rframe_, 0, sizeof(zrtp_hello) + 5 * 8);

    zrtp_hello *msg = (zrtp_hello *)frame_;

    msg->msg_start.header.version = 0;
    msg->msg_start.header.magic   = ZRTP_HEADER_MAGIC;
    msg->msg_start.header.ssrc    = session.ssrc;
    msg->msg_start.header.seq     = session.seq++;

    msg->msg_start.magic  = ZRTP_MSG_MAGIC;
    msg->msg_start.length = len_ - sizeof(uvg_rtp::frame::zrtp_frame);

    memcpy(&msg->msg_start.msgblock, ZRTP_HELLO,                  8);
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
    auto hmac_sha256 = uvg_rtp::crypto::hmac::sha256(session.hash_ctx.o_hash[2], 32);

    hmac_sha256.update((uint8_t *)frame_, 81);
    hmac_sha256.final(mac_full);

    memcpy(&msg->mac, mac_full, sizeof(uint64_t));

    /* Calculate CRC32 of the whole packet (excluding crc) */
    msg->crc = uvg_rtp::crypto::crc32::calculate_crc32((uint8_t *)frame_, len_ - sizeof(uint32_t));

    /* Finally make a copy of the message and save it for later use */
    session.l_msg.hello.first  = len_;
    session.l_msg.hello.second = (uvg_rtp::zrtp_msg::zrtp_hello *)new uint8_t[len_];
    memcpy(session.l_msg.hello.second, msg, len_);
}

uvg_rtp::zrtp_msg::hello::~hello()
{
    LOG_DEBUG("Freeing ZRTP hello message...");
    (void)uvg_rtp::frame::dealloc_frame(frame_);
    (void)uvg_rtp::frame::dealloc_frame(rframe_);
}

rtp_error_t uvg_rtp::zrtp_msg::hello::send_msg(socket_t& socket, sockaddr_in& addr)
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
        log_platform_error("WSASendTo failed");
        return RTP_SEND_ERROR;
    }
#endif

    return RTP_OK;
}

rtp_error_t uvg_rtp::zrtp_msg::hello::parse_msg(uvg_rtp::zrtp_msg::receiver& receiver, zrtp_session_t& session)
{
    ssize_t len = 0;

    if ((len = receiver.get_msg(rframe_, rlen_)) < 0) {
        LOG_ERROR("Failed to get message from ZRTP receiver");
        return RTP_INVALID_VALUE;
    }

    zrtp_hello *msg = (zrtp_hello *)rframe_;

    float version;
    sscanf((const char *)&msg->version, "%f", &version);
    session.capabilities.version = (int)(version * (float)10) * 10;

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
    session.r_msg.hello.second = (uvg_rtp::zrtp_msg::zrtp_hello *)new uint8_t[len];
    memcpy(session.r_msg.hello.second, msg, len);

    return RTP_OK;
}
#endif
