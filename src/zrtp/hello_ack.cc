#include "hello_ack.hh"

#include "uvgrtp/frame.hh"

#include "socket.hh"
#include "zrtp_receiver.hh"
#include "debug.hh"
#include "crypto.hh"

#include <cstring>

#define ZRTP_HELLO_ACK "HelloACK"

uvgrtp::zrtp_msg::hello_ack::hello_ack(zrtp_session_t& session)
{
    allocate_frame(sizeof(zrtp_hello_ack));
    zrtp_hello_ack *msg = (zrtp_hello_ack *)frame_;
    UVG_LOG_DEBUG("Constructing ZRTP Hello ACK");
    set_zrtp_start_base(msg->msg_start, ZRTP_HELLO_ACK);
    msg->msg_start.header.ssrc = htonl(session.ssrc);

    msg->crc = uvgrtp::crypto::crc32::calculate_crc32((uint8_t *)frame_, len_ - sizeof(uint32_t));
}

uvgrtp::zrtp_msg::hello_ack::~hello_ack()
{}
