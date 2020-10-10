#include <cstring>

#include "debug.hh"
#include "zrtp.hh"
#include "zrtp/defines.hh"
#include "zrtp/hello_ack.hh"

#define ZRTP_HELLO_ACK "HelloACK"

uvg_rtp::zrtp_msg::hello_ack::hello_ack()
{
    len_   = sizeof(zrtp_hello_ack);
    frame_ = uvg_rtp::frame::alloc_zrtp_frame(len_);

    zrtp_hello_ack *msg = (zrtp_hello_ack *)frame_;

    memset(msg, 0, sizeof(zrtp_hello_ack));

    msg->msg_start.header.version = 0;
    msg->msg_start.header.magic   = ZRTP_HEADER_MAGIC;

    msg->msg_start.magic  = ZRTP_MSG_MAGIC;
    msg->msg_start.length = 3;

    memcpy(&msg->msg_start.msgblock, ZRTP_HELLO_ACK, 8);

    msg->crc = uvg_rtp::crypto::crc32::calculate_crc32((uint8_t *)frame_, len_ - sizeof(uint32_t));
}

uvg_rtp::zrtp_msg::hello_ack::~hello_ack()
{
    LOG_DEBUG("Freeing ZRTP Hello ACK message...");
    (void)uvg_rtp::frame::dealloc_frame(frame_);
}

rtp_error_t uvg_rtp::zrtp_msg::hello_ack::send_msg(uvg_rtp::socket *socket, sockaddr_in& addr)
{
    rtp_error_t ret;

    if ((ret = socket->sendto(addr, (uint8_t *)frame_, len_, 0, nullptr)) != RTP_OK)
        log_platform_error("Failed to send ZRTP Hello message");

    return ret;
}

rtp_error_t uvg_rtp::zrtp_msg::hello_ack::parse_msg(uvg_rtp::zrtp_msg::receiver& receiver)
{
    (void)receiver;

    return RTP_OK;
}
