#include <cstring>

#include "debug.hh"
#include "zrtp.hh"

#include "zrtp/confack.hh"

#define ZRTP_CONFACK "Conf2ACK"

uvg_rtp::zrtp_msg::confack::confack(zrtp_session_t& session)
{
    LOG_DEBUG("Create ZRTP Conf2ACK message!");

    frame_  = uvg_rtp::frame::alloc_zrtp_frame(sizeof(zrtp_confack));
    rframe_ = uvg_rtp::frame::alloc_zrtp_frame(sizeof(zrtp_confack));

    len_    = sizeof(zrtp_confack);
    rlen_   = sizeof(zrtp_confack);

    memset(frame_,  0, sizeof(zrtp_confack));
    memset(rframe_, 0, sizeof(zrtp_confack));

    zrtp_confack *msg = (zrtp_confack *)frame_;

    msg->msg_start.header.version = 0;
    msg->msg_start.header.magic   = ZRTP_HEADER_MAGIC;

    /* TODO: convert to network byte order */

    msg->msg_start.magic          = ZRTP_MSG_MAGIC;
    msg->msg_start.header.version = 0;
    msg->msg_start.header.magic   = ZRTP_HEADER_MAGIC;
    msg->msg_start.header.ssrc    = session.ssrc;
    msg->msg_start.header.seq     = session.seq++;
    msg->msg_start.length         = len_ - sizeof(zrtp_header);

    memcpy(&msg->msg_start.msgblock, ZRTP_CONFACK, 8);

    /* Calculate CRC32 for the whole ZRTP packet */
    msg->crc = uvg_rtp::crypto::crc32::calculate_crc32((uint8_t *)frame_, len_ - sizeof(uint32_t));
}

uvg_rtp::zrtp_msg::confack::~confack()
{
    LOG_DEBUG("Freeing Conf2ACK message...");

    (void)uvg_rtp::frame::dealloc_frame(frame_);
    (void)uvg_rtp::frame::dealloc_frame(rframe_);
}

rtp_error_t uvg_rtp::zrtp_msg::confack::send_msg(uvg_rtp::socket *socket, sockaddr_in& addr)
{
    rtp_error_t ret;

    if ((ret = socket->sendto(addr, (uint8_t *)frame_, len_, 0, nullptr)) != RTP_OK)
        log_platform_error("Failed to send ZRTP Hello message");

    return ret;
}

rtp_error_t uvg_rtp::zrtp_msg::confack::parse_msg(uvg_rtp::zrtp_msg::receiver& receiver)
{
    ssize_t len = 0;

    if ((len = receiver.get_msg(rframe_, rlen_)) < 0) {
        LOG_ERROR("Failed to get message from ZRTP receiver");
        return RTP_INVALID_VALUE;
    }

    return RTP_OK;
}
