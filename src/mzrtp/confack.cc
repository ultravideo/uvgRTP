#include <cstring>

#include "debug.hh"
#include "zrtp.hh"

#include "mzrtp/confack.hh"

#define ZRTP_CONFACK "Conf2ACK"

kvz_rtp::zrtp_msg::confack::confack()
{
    LOG_DEBUG("Create ZRTP Conf2ACK message!");

    frame_  = kvz_rtp::frame::alloc_zrtp_frame(sizeof(zrtp_confack));
    rframe_ = kvz_rtp::frame::alloc_zrtp_frame(sizeof(zrtp_confack));

    len_    = sizeof(zrtp_confack);
    rlen_   = sizeof(zrtp_confack);

    memset(frame_,  0, sizeof(zrtp_confack));
    memset(rframe_, 0, sizeof(zrtp_confack));

    zrtp_confack *msg = (zrtp_confack *)frame_;

    msg->msg_start.header.version = 0;
    msg->msg_start.header.magic   = ZRTP_HEADER_MAGIC;

    /* TODO: convert to network byte order */

    msg->msg_start.magic  = ZRTP_MSG_MAGIC;
    msg->msg_start.length = len_ - sizeof(zrtp_header);

    memcpy(&msg->msg_start.msgblock, ZRTP_CONFACK, 8);

    /* TODO: everything */
}

kvz_rtp::zrtp_msg::confack::~confack()
{
    LOG_DEBUG("Freeing Conf2ACK message...");

    (void)kvz_rtp::frame::dealloc_frame(frame_);
    (void)kvz_rtp::frame::dealloc_frame(rframe_);
}

rtp_error_t kvz_rtp::zrtp_msg::confack::send_msg(socket_t& socket, sockaddr_in& addr)
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

rtp_error_t kvz_rtp::zrtp_msg::confack::parse_msg(kvz_rtp::zrtp_msg::receiver& receiver)
{
    ssize_t len = 0;

    if ((len = receiver.get_msg(rframe_, rlen_)) < 0) {
        LOG_ERROR("Failed to get message from ZRTP receiver");
        return RTP_INVALID_VALUE;
    }

    return RTP_OK;
}
