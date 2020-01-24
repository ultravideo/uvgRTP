#include <cstring>

#include "debug.hh"
#include "zrtp.hh"

#include "mzrtp/commit.hh"

#define ZRTP_COMMIT "Commit  "

kvz_rtp::zrtp_msg::commit::commit(zrtp_session_t& session)
{
    LOG_DEBUG("Create ZRTP Commit message!");

    frame_  = kvz_rtp::frame::alloc_zrtp_frame(sizeof(zrtp_commit));
    len_    = sizeof(zrtp_commit);
    rframe_ = kvz_rtp::frame::alloc_zrtp_frame(sizeof(zrtp_commit));
    rlen_   = sizeof(zrtp_commit);

    memset(frame_,  0, sizeof(zrtp_commit));
    memset(rframe_, 0, sizeof(zrtp_commit));

    zrtp_commit *msg = (zrtp_commit *)frame_;

    msg->msg_start.header.version = 0;
    msg->msg_start.header.magic   = ZRTP_HEADER_MAGIC;

    /* TODO: convert to network byte order */

    msg->msg_start.magic  = ZRTP_MSG_MAGIC;
    msg->msg_start.length = len_ - sizeof(zrtp_header);

    memcpy(&msg->msg_start.msgblock, ZRTP_COMMIT, 8);

    /* TODO: hash image */
    /* TODO: zid */

    msg->sas_type           = session.sas_type;
    msg->hash_algo          = session.hash_algo;
    msg->cipher_algo        = session.cipher_algo;
    msg->auth_tag_type      = session.auth_tag_type;
    msg->key_agreement_type = session.key_agreement_type;

    /* TODO: hvi */
    msg->hvi[0]  = session.hvi[0];

    /* TODO: mac */
}

kvz_rtp::zrtp_msg::commit::~commit()
{
    LOG_DEBUG("Freeing Commit message...");

    (void)kvz_rtp::frame::dealloc_frame(frame_);
    (void)kvz_rtp::frame::dealloc_frame(rframe_);
}

rtp_error_t kvz_rtp::zrtp_msg::commit::send_msg(socket_t& socket, sockaddr_in& addr)
{
#ifdef __linux
    if (::sendto(socket, (void *)frame_, len_, 0, (const struct sockaddr *)&addr, (socklen_t)sizeof(addr)) < 0) {
        LOG_ERROR("Failed to send ZRTP Hello message: %s!", strerror(errno));
        return RTP_SEND_ERROR;
    }
#else
    /* TODO:  */
#endif

    return RTP_OK;
}

rtp_error_t kvz_rtp::zrtp_msg::commit::parse_msg(kvz_rtp::zrtp_msg::receiver& receiver, zrtp_session_t& session)
{
    ssize_t len = 0;

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

    memcpy(session.hvi, msg->hvi, sizeof(uint32_t) * 8);

    /* TODO: validate mac */

    return RTP_OK;
}
