#include <cstring>

#include "debug.hh"
#include "zrtp.hh"
#include "mzrtp/defines.hh"
#include "mzrtp/hello_ack.hh"

#define ZRTP_HELLO_ACK "HelloACK"

kvz_rtp::zrtp_msg::hello_ack::hello_ack()
{
    len_   = sizeof(zrtp_hello_ack);
    frame_ = kvz_rtp::frame::alloc_zrtp_frame(len_);

    zrtp_hello_ack *msg = (zrtp_hello_ack *)frame_;

    msg->msg_start.header.version = 0;
    msg->msg_start.header.magic   = ZRTP_HEADER_MAGIC;

    msg->msg_start.magic  = ZRTP_MSG_MAGIC;
    msg->msg_start.length = 3;

    memcpy(&msg->msg_start.msgblock, ZRTP_HELLO_ACK, 8);
}

kvz_rtp::zrtp_msg::hello_ack::~hello_ack()
{
    LOG_DEBUG("Freeing ZRTP Hello ACK message...");
    (void)kvz_rtp::frame::dealloc_frame(frame_);
}

rtp_error_t kvz_rtp::zrtp_msg::hello_ack::send_msg(socket_t& socket, sockaddr_in& addr)
{
#ifdef __linux
    if (::sendto(socket, (void *)frame_, len_, 0, (const struct sockaddr *)&addr, (socklen_t)sizeof(addr)) < 0) {
        LOG_ERROR("Failed to send ZRTP Hello ACK message: %s!", strerror(errno));
        return RTP_SEND_ERROR;
    }
#else
    /* TODO:  */
#endif

    return RTP_OK;
}

rtp_error_t kvz_rtp::zrtp_msg::hello_ack::parse_msg(kvz_rtp::zrtp_msg::receiver& receiver)
{
    (void)receiver;

    return RTP_OK;
}
