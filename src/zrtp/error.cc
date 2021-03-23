#include <cstring>

#include "debug.hh"
#include "zrtp.hh"
#include "zrtp/defines.hh"
#include "zrtp/error.hh"

#define ZRTP_ERROR "Error   "

uvgrtp::zrtp_msg::error::error(int error_code)
{
    len_   = sizeof(zrtp_error);
    frame_ = uvgrtp::frame::alloc_zrtp_frame(len_);

    zrtp_error *msg = (zrtp_error *)frame_;

    memset(msg, 0, sizeof(zrtp_error));

    msg->msg_start.header.version = 0;
    msg->msg_start.header.magic   = ZRTP_HEADER_MAGIC;

    msg->msg_start.magic  = ZRTP_MSG_MAGIC;
    msg->msg_start.length = 4;

    memcpy(&msg->msg_start.msgblock, ZRTP_ERROR, 8);

    msg->error = error_code;

    msg->crc = uvgrtp::crypto::crc32::calculate_crc32((uint8_t *)frame_, len_ - sizeof(uint32_t));
}

uvgrtp::zrtp_msg::error::~error()
{
    LOG_DEBUG("Freeing ZRTP Error message...");
    (void)uvgrtp::frame::dealloc_frame(frame_);
}

rtp_error_t uvgrtp::zrtp_msg::error::send_msg(uvgrtp::socket *socket, sockaddr_in& addr)
{
    rtp_error_t ret;

    if ((ret = socket->sendto(addr, (uint8_t *)frame_, len_, 0, nullptr)) != RTP_OK)
        log_platform_error("Failed to send ZRTP Hello message");

    return ret;
}

rtp_error_t uvgrtp::zrtp_msg::error::parse_msg(uvgrtp::zrtp_msg::receiver& receiver)
{
    (void)receiver;

    /* TODO:  */

    return RTP_OK;
}
