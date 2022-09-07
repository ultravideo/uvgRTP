#include "error.hh"

#include "uvgrtp/frame.hh"

#include "zrtp_receiver.hh"
#include "../debug.hh"
#include "../crypto.hh"

#include <cstring>

#define ZRTP_ERROR "Error   "

uvgrtp::zrtp_msg::error::error(int error_code):
    zrtp_message()
{
    allocate_frame(sizeof(zrtp_error));
    zrtp_error* msg = (zrtp_error*)frame_;

    UVG_LOG_DEBUG("Constructing ZRTP error");
    set_zrtp_start_base(msg->msg_start, ZRTP_ERROR);

    msg->error = error_code;
    msg->crc = uvgrtp::crypto::crc32::calculate_crc32((uint8_t *)frame_, len_ - sizeof(uint32_t));
}

uvgrtp::zrtp_msg::error::~error()
{}

rtp_error_t uvgrtp::zrtp_msg::error::parse_msg(uvgrtp::zrtp_msg::receiver& receiver,
                                               zrtp_session_t& session)
{
    (void)receiver;
    (void)session;

    /* TODO:  */

    return RTP_OK;
}
