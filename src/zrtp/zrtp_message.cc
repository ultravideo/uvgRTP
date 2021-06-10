#include "zrtp_message.hh"

#include "frame.hh"
#include "socket.hh"

#include "debug.hh"

uvgrtp::zrtp_msg::zrtp_message::zrtp_message():
    frame_(nullptr),
    rframe_(nullptr),
    len_(0),
    rlen_(0)
{}


uvgrtp::zrtp_msg::zrtp_message::~zrtp_message()
{
  LOG_DEBUG("Freeing zrtp message...");

  if (frame_)
    (void)uvgrtp::frame::dealloc_frame(frame_);

  if (rframe_)
    (void)uvgrtp::frame::dealloc_frame(rframe_);
}

rtp_error_t uvgrtp::zrtp_msg::zrtp_message::send_msg(uvgrtp::socket *socket, sockaddr_in& addr)
{
    rtp_error_t ret;

    if ((ret = socket->sendto(addr, (uint8_t *)frame_, len_, 0, nullptr)) != RTP_OK)
        log_platform_error("Failed to send ZRTP message");

    return ret;
}

void uvgrtp::zrtp_msg::zrtp_message::allocate_frame(size_t frame_size)
{
    frame_ = uvgrtp::frame::alloc_zrtp_frame(frame_size);
    len_ = frame_size;
    memset(frame_, 0, frame_size);
}

void uvgrtp::zrtp_msg::zrtp_message::allocate_rframe(size_t frame_size)
{
    rframe_ = uvgrtp::frame::alloc_zrtp_frame(frame_size);
    rlen_ = frame_size;
    memset(rframe_, 0, frame_size);
}

void uvgrtp::zrtp_msg::zrtp_message::set_zrtp_start_base(uvgrtp::zrtp_msg::zrtp_msg& start,
    std::string msgblock)
{
    start.header.version = 0;
    start.header.magic = ZRTP_HEADER_MAGIC;
    start.magic = ZRTP_MSG_MAGIC;
    start.length = (uint16_t)len_ - (uint16_t)sizeof(zrtp_header);

    memcpy(&start.msgblock, msgblock.c_str(), 8);

    LOG_DEBUG("Constructed ZRTP header. Type: " + msgblock.c_str() + " Length: %u", start.length);
}

void uvgrtp::zrtp_msg::zrtp_message::set_zrtp_start(uvgrtp::zrtp_msg::zrtp_msg& start,
    zrtp_session_t& session, std::string msgblock)
{
    /* TODO: convert to network byte order */
    set_zrtp_start_base(start, msgblock);

    start.header.ssrc = session.ssrc;
    start.header.seq = session.seq++;
}
