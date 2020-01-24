#include <cstring>

#include "debug.hh"
#include "zrtp.hh"

#include "mzrtp/confirm.hh"

#define ZRTP_CONFRIM1 "Confirm1"
#define ZRTP_CONFRIM2 "Confirm2"

kvz_rtp::zrtp_msg::confirm::confirm(int part)
{
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

    /* TODO: convert to network byte order */

    msg->msg_start.magic  = ZRTP_MSG_MAGIC;
    msg->msg_start.length = len_ - sizeof(zrtp_header);

    memcpy(&msg->msg_start.msgblock, (part == 1) ? ZRTP_CONFRIM1 : ZRTP_CONFRIM2, 8);

    /* TODO: everything */
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
    /* TODO:  */
#endif

    return RTP_OK;
}

rtp_error_t kvz_rtp::zrtp_msg::confirm::parse_msg(kvz_rtp::zrtp_msg::receiver& receiver)
{
    ssize_t len = 0;

    if ((len = receiver.get_msg(rframe_, rlen_)) < 0) {
        LOG_ERROR("Failed to get message from ZRTP receiver");
        return RTP_INVALID_VALUE;
    }

    return RTP_OK;
}
