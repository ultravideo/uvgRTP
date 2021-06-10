#include "error.hh"

#include "zrtp_receiver.hh"

#include "../zrtp.hh"
#include "crypto.hh"
#include "socket.hh"
#include "frame.hh"

#include "debug.hh"


#include <cstring>

#define ZRTP_ERROR "Error   "

uvgrtp::zrtp_msg::error::error(int error_code):
    zrtp_message()
{
    allocate_frame(sizeof(zrtp_error));
    zrtp_error* msg = (zrtp_error*)frame_;

    set_zrtp_start_base(msg->msg_start, ZRTP_ERROR);

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
