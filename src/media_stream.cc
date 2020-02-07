#include "media_stream.hh"

kvz_rtp::media_stream::media_stream(std::string addr, int src_port, int dst_port, rtp_format_t fmt, int flags):
    conn_(nullptr),
    /* writer_(nullptr), */
    /* reader_(nullptr), */
    srtp_(nullptr)
{
    fmt_      = fmt;
    addr_     = addr;
    flags_    = flags;
    src_port_ = src_port;
    dst_port_ = dst_port;
}

kvz_rtp::media_stream::~media_stream()
{
}

rtp_error_t kvz_rtp::media_stream::init()
{
    conn_ = new kvz_rtp::connection(addr_, src_port_, dst_port_, fmt_, flags_);
    rtp_  = new kvz_rtp::rtp(fmt_);

    conn_->init();

    sender_   = new kvz_rtp::sender(conn_->get_socket(), conn_->get_ctx_conf(), fmt_, rtp_);
    receiver_ = new kvz_rtp::receiver(conn_->get_socket(), conn_->get_ctx_conf(), fmt_, rtp_);

    sender_->init();
    receiver_->start();

    return RTP_OK;
}

rtp_error_t kvz_rtp::media_stream::init(kvz_rtp::zrtp& zrtp)
{
    conn_ = new kvz_rtp::connection(addr_, src_port_, dst_port_, fmt_, flags_);
}

rtp_error_t kvz_rtp::media_stream::push_frame(uint8_t *data, size_t data_len, int flags)
{
    return sender_->push_frame(data, data_len, flags);
}

rtp_error_t kvz_rtp::media_stream::push_frame(std::unique_ptr<uint8_t[]> data, size_t data_len, int flags)
{
    return sender_->push_frame(std::move(data), data_len, flags);
}

rtp_error_t kvz_rtp::media_stream::pull_frame()
{
}

