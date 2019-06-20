#include "multicast.hh"

kvz_rtp::multicast::multicast()
{
}

kvz_rtp::multicast::~multicast()
{
}

rtp_error_t kvz_rtp::multicast::join_multicast(kvz_rtp::connection *conn)
{
    (void)conn;

    return RTP_OK;
}

rtp_error_t kvz_rtp::multicast::leave_multicast(kvz_rtp::connection *conn)
{
    (void)conn;

    return RTP_OK;
}

rtp_error_t kvz_rtp::multicast::push_frame_multicast(
    kvz_rtp::connection *sender,
    uint8_t *data, uint32_t data_len,
    rtp_format_t fmt, uint32_t timestamp
)
{
    (void)sender, (void)data, (void)data_len, (void)fmt, (void)timestamp;

    return RTP_OK;
}

rtp_error_t kvz_rtp::multicast::push_frame_multicast(kvz_rtp::connection *sender, kvz_rtp::frame::rtp_frame *frame)
{
    (void)sender, (void)frame;

    return RTP_OK;
}
