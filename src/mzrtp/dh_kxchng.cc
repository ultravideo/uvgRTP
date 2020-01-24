#include <cstring>

#include "debug.hh"
#include "zrtp.hh"
#include "mzrtp/dh_kxchng.hh"
#include "mzrtp/defines.hh"

#define ZRTP_DH_PART1       "DHPart1 "
#define ZRTP_DH_PART2       "DHPart2 "

kvz_rtp::zrtp_msg::dh_key_exchange::dh_key_exchange(int part)
{
    LOG_DEBUG("Create ZRTP Commit message!");

    frame_  = kvz_rtp::frame::alloc_zrtp_frame(sizeof(zrtp_dh));
    len_    = sizeof(zrtp_dh);
    rframe_ = kvz_rtp::frame::alloc_zrtp_frame(sizeof(zrtp_dh));
    rlen_   = sizeof(zrtp_dh);

    memset(frame_,  0, sizeof(zrtp_dh));
    memset(rframe_, 0, sizeof(zrtp_dh));

    zrtp_dh *msg = (zrtp_dh *)frame_;

    msg->msg_start.header.version = 0;
    msg->msg_start.header.magic   = ZRTP_HEADER_MAGIC;

    /* TODO: convert to network byte order */

    msg->msg_start.magic  = ZRTP_MSG_MAGIC;
    msg->msg_start.length = len_ - sizeof(zrtp_header);

    memcpy(&msg->msg_start.msgblock, (part == 1) ? ZRTP_DH_PART1 : ZRTP_DH_PART2, 8);
}

kvz_rtp::zrtp_msg::dh_key_exchange::~dh_key_exchange()
{
    LOG_DEBUG("Freeing DHPartN message...");

    (void)kvz_rtp::frame::dealloc_frame(frame_);
    (void)kvz_rtp::frame::dealloc_frame(rframe_);
}

rtp_error_t kvz_rtp::zrtp_msg::dh_key_exchange::send_msg(socket_t& socket, sockaddr_in& addr)
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

rtp_error_t kvz_rtp::zrtp_msg::dh_key_exchange::parse_msg(kvz_rtp::zrtp_msg::receiver& receiver, kvz_rtp::zrtp_dh_t& dh)
{
    LOG_DEBUG("Parsing DHPart1/DHPart2 message...");

    ssize_t len = 0;

    if ((len = receiver.get_msg(rframe_, rlen_)) < 0) {
        LOG_ERROR("Failed to get message from ZRTP receiver");
        return RTP_INVALID_VALUE;
    }

    zrtp_dh *msg = (zrtp_dh *)rframe_;

    memcpy(dh.retained1,  msg->rs1_id,     sizeof(uint32_t) * 2);
    memcpy(dh.retained2,  msg->rs2_id,     sizeof(uint32_t) * 2);
    memcpy(dh.aux_secret, msg->aux_secret, sizeof(uint32_t) * 2);
    memcpy(dh.pbx_secret, msg->pbx_secret, sizeof(uint32_t) * 2);

    return RTP_OK;
}
