#include <cstring>

#include "debug.hh"
#include "zrtp.hh"
#include "mzrtp/hello.hh"
#include "mzrtp/defines.hh"

#define ZRTP_VERSION     "1.10"
#define ZRTP_HELLO       "Hello   "
#define ZRTP_CLIENT_ID   "kvzRTP,UVG,TUNI "

using namespace kvz_rtp::zrtp_msg;

kvz_rtp::zrtp_msg::hello::hello(zrtp_session_t& session)
{
    /* temporary storage for the full hmac hash */
    uint8_t mac_full[32];

    /* We support only the mandatory algorithms etc. defined in RFC 6189
     * so for us all the hash algos and key agreement types are set to zero  */
    size_t common_size = sizeof(zrtp_hello) - sizeof(zrtp_header);
    size_t our_size    = common_size;

    /* assume remote supports everything so allocate space for all possible elements */
    size_t their_size = common_size + 5 * 8 * sizeof(uint32_t);

    frame_  = kvz_rtp::frame::alloc_zrtp_frame(our_size);
    rframe_ = kvz_rtp::frame::alloc_zrtp_frame(their_size);

    len_  = sizeof(kvz_rtp::frame::zrtp_frame) + our_size;
    rlen_ = sizeof(kvz_rtp::frame::zrtp_frame) + their_size;

    zrtp_hello *msg = (zrtp_hello *)frame_;

    msg->msg_start.header.version = 0;
    msg->msg_start.header.magic   = ZRTP_HEADER_MAGIC;
    msg->msg_start.header.ssrc    = session.ssrc;
    msg->msg_start.header.seq     = session.seq++;

    msg->msg_start.magic  = ZRTP_MSG_MAGIC;
    msg->msg_start.length = our_size;

    memcpy(&msg->msg_start.msgblock, ZRTP_HELLO,                8);
    memcpy(&msg->version,            ZRTP_VERSION,              4);
    memcpy(&msg->client,             ZRTP_CLIENT_ID,           16);
    memcpy(&msg->hash,               session.hashes[3],        32); /* 256 bits */
    memcpy(&msg->zid,                session.capabilities.zid, 12); /* 96 bits */

    msg->zero   = 0;
    msg->s      = 0;
    msg->m      = 0;
    msg->p      = 0;
    msg->unused = 0;
    msg->hc     = 0;
    msg->ac     = 0;
    msg->kc     = 0;
    msg->sc     = 0;

    auto hmac_sha256 = kvz_rtp::crypto::hmac::sha256(session.hashes[2], 32);

    hmac_sha256.update((uint8_t *)frame_, our_size - 8);
    hmac_sha256.final(mac_full);

    memcpy(&msg->mac, mac_full, sizeof(uint64_t));
}

kvz_rtp::zrtp_msg::hello::~hello()
{
    LOG_DEBUG("Freeing ZRTP hello message...");
    (void)kvz_rtp::frame::dealloc_frame(frame_);
}

rtp_error_t kvz_rtp::zrtp_msg::hello::send_msg(socket_t& socket, sockaddr_in& addr)
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

rtp_error_t kvz_rtp::zrtp_msg::hello::parse_msg(kvz_rtp::zrtp_msg::receiver& receiver, zrtp_session_t& session)
{
    ssize_t len = 0;

    if ((len = receiver.get_msg(rframe_, rlen_)) < 0) {
        LOG_ERROR("Failed to get message from ZRTP receiver");
        return RTP_INVALID_VALUE;
    }

    zrtp_hello *msg = (zrtp_hello *)rframe_;

    /* Make sure the version of the message is the one we support.
     * If it's not, there's no reason to parse the message any further */
    if (memcmp(&msg->version, ZRTP_VERSION, 4) == 0)
        session.rcapab.version = 110;
    else
        return RTP_NOT_SUPPORTED;

    /* TODO: collect preferred algorithms from hello message */
    /* TODO: not needed until new release */

    if (msg->hc != 0) {
    }

    if (msg->cc != 0) {
    }

    if (msg->ac != 0) {
    }

    if (msg->kc != 0) {
    }

    if (msg->sc != 0) {
    }

    /* finally add mandatory algorithms required by the specification to remote capabilities */
    session.rcapab.hash_algos.push_back(S256);
    session.rcapab.cipher_algos.push_back(AES1);
    session.rcapab.auth_tags.push_back(HS32);
    session.rcapab.auth_tags.push_back(HS80);
    session.rcapab.key_agreements.push_back(DH3k);
    session.rcapab.sas_types.push_back(B32);

    /* Save the MAC value so we can check if later */
    memcpy(&session.remote_macs[0],   &msg->mac,  8);
    memcpy(&session.remote_hashes[3], msg->hash, 32);

    session.cctx.sha256->update((uint8_t *)msg, msg->msg_start.length);

    return RTP_OK;
}
