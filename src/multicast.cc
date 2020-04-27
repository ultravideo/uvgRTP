#include "multicast.hh"

uvg_rtp::multicast::multicast()
{
}

uvg_rtp::multicast::~multicast()
{
}

rtp_error_t uvg_rtp::multicast::join_multicast(uvg_rtp::connection *conn)
{
    (void)conn;

    return RTP_OK;
}

rtp_error_t uvg_rtp::multicast::leave_multicast(uvg_rtp::connection *conn)
{
    (void)conn;

    return RTP_OK;
}

rtp_error_t uvg_rtp::multicast::push_frame_multicast(
    uvg_rtp::connection *sender,
    uint8_t *data, uint32_t data_len,
    rtp_format_t fmt, uint32_t timestamp
)
{
    (void)sender, (void)data, (void)data_len, (void)fmt, (void)timestamp;

    return RTP_OK;
}

rtp_error_t uvg_rtp::multicast::push_frame_multicast(uvg_rtp::connection *sender, uvg_rtp::frame::rtp_frame *frame)
{
    (void)sender, (void)frame;

    return RTP_OK;
}
