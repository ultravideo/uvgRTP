#ifdef __RTP_CRYPTO__
#include <cstring>

#include "debug.hh"
#include "zrtp.hh"
#include "mzrtp/defines.hh"
#include "mzrtp/hello_ack.hh"

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

    uvg_rtp::crypto::crc32::get_crc32((uint8_t *)frame_, len_ - 4, &msg->crc);
}

uvg_rtp::zrtp_msg::hello_ack::~hello_ack()
{
    LOG_DEBUG("Freeing ZRTP Hello ACK message...");
    (void)uvg_rtp::frame::dealloc_frame(frame_);
}

rtp_error_t uvg_rtp::zrtp_msg::hello_ack::send_msg(socket_t& socket, sockaddr_in& addr)
{
#ifdef __linux
    if (::sendto(socket, (void *)frame_, len_, 0, (const struct sockaddr *)&addr, (socklen_t)sizeof(addr)) < 0) {
        LOG_ERROR("Failed to send ZRTP Hello ACK message: %s!", strerror(errno));
        return RTP_SEND_ERROR;
    }
#else
    DWORD sent_bytes;
    WSABUF data_buf;

    data_buf.buf = (char *)frame_;
    data_buf.len = len_;

    if (WSASendTo(socket, &data_buf, 1, NULL, 0, (const struct sockaddr *)&addr, sizeof(addr), nullptr, nullptr) == -1) {
        win_get_last_error();

        return RTP_SEND_ERROR;
    }
#endif

    return RTP_OK;
}

rtp_error_t uvg_rtp::zrtp_msg::hello_ack::parse_msg(uvg_rtp::zrtp_msg::receiver& receiver)
{
    (void)receiver;

    return RTP_OK;
}
#endif
