#include "zrtp_message.hh"

#include "uvgrtp/frame.hh"

#include "socket.hh"
#include "debug.hh"

#include <string>
#ifdef _WIN32
#include <ws2ipdef.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#endif

uvgrtp::zrtp_msg::zrtp_message::zrtp_message():
    frame_(nullptr),
    rframe_(nullptr),
    len_(0),
    rlen_(0)
{}


uvgrtp::zrtp_msg::zrtp_message::~zrtp_message()
{
    //UVG_LOG_DEBUG("Freeing zrtp message...");

    if (frame_)
        (void)uvgrtp::frame::dealloc_frame((uvgrtp::frame::zrtp_frame*)frame_);

    if (rframe_)
        (void)uvgrtp::frame::dealloc_frame((uvgrtp::frame::zrtp_frame*)rframe_);
}

rtp_error_t uvgrtp::zrtp_msg::zrtp_message::send_msg(std::shared_ptr<uvgrtp::socket> socket, sockaddr_in& addr, sockaddr_in6& addr6)
{
    rtp_error_t ret;
    if ((ret = socket->sendto(addr, addr6, (uint8_t*)frame_, len_, 0, nullptr)) != RTP_OK)
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
    start.header.magic = htonl(ZRTP_MAGIC);
    start.preamble = ZRTP_PREAMBLE;
    start.length = packet_to_header_len(len_);
    memcpy(&start.msgblock, msgblock.c_str(), 8);

    UVG_LOG_DEBUG("Constructed ZRTP header. Size: %lu, Length-field: %u", len_, start.length);
}

void uvgrtp::zrtp_msg::zrtp_message::set_zrtp_start(uvgrtp::zrtp_msg::zrtp_msg& start,
    zrtp_session_t& session, std::string msgblock)
{
    /* TODO: convert to network byte order */
    set_zrtp_start_base(start, msgblock);

    start.header.ssrc = htonl(session.ssrc);
    start.header.seq = session.seq++;
}

ssize_t uvgrtp::zrtp_msg::zrtp_message::header_length_to_packet(uint16_t header_len)
{
    return ((ssize_t)header_len + 1)*4 + sizeof(zrtp_header);
}

uint16_t uvgrtp::zrtp_msg::zrtp_message::packet_to_header_len(ssize_t packet)
{
    if (packet % 4 != 0)
    {
        std::string printText = "ZRTP message length is not divisible by 32-bit word";
        log_platform_error(printText.c_str());
        return 0;
    }

    // convert size into 32-bit words and minus the size of crc
    return ((uint16_t)packet - (uint16_t)sizeof(zrtp_header)) / 4 - 1;
}