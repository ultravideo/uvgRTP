#include "confack.hh"

#include "../zrtp.hh"
#include "crypto.hh"
#include "frame.hh"
#include "socket.hh"
#include "debug.hh"

#include <cstring>


#define ZRTP_CONFACK "Conf2ACK"

uvgrtp::zrtp_msg::confack::confack(zrtp_session_t& session):
    zrtp_message()
{
    LOG_DEBUG("Create ZRTP Conf2ACK message!");

    allocate_frame(sizeof(zrtp_confack));
    zrtp_confack* msg = (zrtp_confack*)frame_;
    set_zrtp_start(msg->msg_start, session, ZRTP_CONFACK);

    /* Calculate CRC32 for the whole ZRTP packet */
    msg->crc = uvgrtp::crypto::crc32::calculate_crc32((uint8_t *)frame_, len_ - sizeof(uint32_t));
}

uvgrtp::zrtp_msg::confack::~confack()
{}

rtp_error_t uvgrtp::zrtp_msg::confack::parse_msg(uvgrtp::zrtp_msg::receiver& receiver,
    zrtp_session_t& session)
{
    ssize_t len = 0;
    allocate_rframe(sizeof(zrtp_confack));

    if ((len = receiver.get_msg(rframe_, rlen_)) < 0) {
        LOG_ERROR("Failed to get message from ZRTP receiver");
        return RTP_INVALID_VALUE;
    }

    return RTP_OK;
}
