#ifdef __RTP_CRYPTO__
#include <cstring>

#include "debug.hh"
#include "zrtp.hh"
#include "mzrtp/defines.hh"
#include "mzrtp/error.hh"

#define ZRTP_ERROR "Error   "

kvz_rtp::zrtp_msg::error::error(int error_code)
{
    len_   = sizeof(zrtp_error);
    frame_ = kvz_rtp::frame::alloc_zrtp_frame(len_);

    zrtp_error *msg = (zrtp_error *)frame_;

    memset(msg, 0, sizeof(zrtp_error));

    msg->msg_start.header.version = 0;
    msg->msg_start.header.magic   = ZRTP_HEADER_MAGIC;

    msg->msg_start.magic  = ZRTP_MSG_MAGIC;
    msg->msg_start.length = 4;

    memcpy(&msg->msg_start.msgblock, ZRTP_ERROR, 8);

    msg->error = error_code;

    kvz_rtp::crypto::crc32::get_crc32((uint8_t *)frame_, len_ - 4, &msg->crc);
}

kvz_rtp::zrtp_msg::error::~error()
{
    LOG_DEBUG("Freeing ZRTP Error message...");
    (void)kvz_rtp::frame::dealloc_frame(frame_);
}

rtp_error_t kvz_rtp::zrtp_msg::error::send_msg(socket_t& socket, sockaddr_in& addr)
{
#ifdef __linux
    if (::sendto(socket, (void *)frame_, len_, 0, (const struct sockaddr *)&addr, (socklen_t)sizeof(addr)) < 0) {
        LOG_ERROR("Failed to send ZRTP Error message: %s!", strerror(errno));
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

rtp_error_t kvz_rtp::zrtp_msg::error::parse_msg(kvz_rtp::zrtp_msg::receiver& receiver)
{
    (void)receiver;

    return RTP_OK;
}
#endif
