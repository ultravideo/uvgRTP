#include "multicast.hh"

uvgrtp::multicast::multicast()
{
}

uvgrtp::multicast::~multicast()
{
}

rtp_error_t uvgrtp::multicast::join_multicast(uvgrtp::connection *conn)
{
    (void)conn;

    return RTP_OK;
}

rtp_error_t uvgrtp::multicast::leave_multicast(uvgrtp::connection *conn)
{
    (void)conn;

    return RTP_OK;
}

rtp_error_t uvgrtp::multicast::push_frame_multicast(
    uvgrtp::connection *sender,
    uint8_t *data, uint32_t data_len,
    rtp_format_t fmt, uint32_t timestamp
)
{
    (void)sender, (void)data, (void)data_len, (void)fmt, (void)timestamp;

    return RTP_OK;
}

rtp_error_t uvgrtp::multicast::push_frame_multicast(uvgrtp::connection *sender, uvgrtp::frame::rtp_frame *frame)
{
    (void)sender, (void)frame;

    return RTP_OK;
}
