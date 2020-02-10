#include "media_stream.hh"

kvz_rtp::media_stream::media_stream(std::string addr, int src_port, int dst_port, rtp_format_t fmt, int flags):
    socket_(),
    srtp_(nullptr),
    media_config_(nullptr)
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

kvz_rtp::frame::rtp_frame *kvz_rtp::media_stream::pull_frame()
{
    return receiver_->pull_frame();
}

rtp_error_t kvz_rtp::media_stream::install_receive_hook(void *arg, void (*hook)(void *, kvz_rtp::frame::rtp_frame *))
{
    if (!hook)
        return RTP_INVALID_VALUE;

    receiver_->install_recv_hook(arg, hook);

    return RTP_OK;
}

rtp_error_t kvz_rtp::media_stream::install_deallocation_hook(void (*hook)(void *))
{
    if (!hook)
        return RTP_INVALID_VALUE;

    sender_->install_dealloc_hook(hook);

    return RTP_OK;
}

void kvz_rtp::media_stream::set_media_config(void *config)
{
    media_config_ = config;
}

void *kvz_rtp::media_stream::get_media_config()
{
    return media_config_;
}

rtp_ctx_conf_t& kvz_rtp::media_stream::get_ctx_config()
{
    return ctx_config_;
}

rtp_error_t kvz_rtp::media_stream::configure_ctx(int flag, ssize_t value)
{
    if (flag < 0 || flag >= RCC_LAST || value < 0)
        return RTP_INVALID_VALUE;

    ctx_config_.ctx_values[flag] = value;

    return RTP_OK;
}

rtp_error_t kvz_rtp::media_stream::configure_ctx(int flag)
{
    if (flag < 0 || flag >= RCE_LAST)
        return RTP_INVALID_VALUE;

    ctx_config_.flags |= flag;

    return RTP_OK;
}
