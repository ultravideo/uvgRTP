#include "uvgrtp/media_stream.hh"

#include "media_stream_internal.hh"


uvgrtp::media_stream::media_stream(std::string cname, std::string remote_addr,
    std::string local_addr, uint16_t src_port, uint16_t dst_port, rtp_format_t fmt,
    std::shared_ptr<uvgrtp::socketfactory> sfp, int rce_flags):
    impl_(std::make_unique<media_stream_internal>(
        std::move(cname), std::move(remote_addr), std::move(local_addr),
        src_port, dst_port, fmt, std::move(sfp), rce_flags))
{}

uvgrtp::media_stream::~media_stream()
{}

rtp_error_t uvgrtp::media_stream::start_zrtp()
{
    return impl_->start_zrtp();
}

rtp_error_t uvgrtp::media_stream::add_srtp_ctx(uint8_t *key, uint8_t *salt)
{
    return impl_->add_srtp_ctx(key, salt);
}

rtp_error_t uvgrtp::media_stream::push_frame(uint8_t *data, size_t data_len, int rtp_flags)
{
    return impl_->push_frame(data, data_len, rtp_flags);
}

rtp_error_t uvgrtp::media_stream::push_frame(std::unique_ptr<uint8_t[]> data, size_t data_len, int rtp_flags)
{
    return impl_->push_frame(std::move(data), data_len, rtp_flags);
}

rtp_error_t uvgrtp::media_stream::push_frame(uint8_t *data, size_t data_len, uint32_t ts, int rtp_flags)
{
    return impl_->push_frame(data, data_len, ts, rtp_flags);
}

rtp_error_t uvgrtp::media_stream::push_frame(uint8_t* data, size_t data_len, uint32_t ts, uint64_t ntp_ts, int rtp_flags)
{
    return impl_->push_frame(data, data_len, ts, ntp_ts, rtp_flags);
}

rtp_error_t uvgrtp::media_stream::push_frame(std::unique_ptr<uint8_t[]> data, size_t data_len, uint32_t ts, int rtp_flags)
{
    return impl_->push_frame(std::move(data), data_len, ts, rtp_flags);
}

rtp_error_t uvgrtp::media_stream::push_frame(std::unique_ptr<uint8_t[]> data, size_t data_len, uint32_t ts, uint64_t ntp_ts, int rtp_flags)
{
    return impl_->push_frame(std::move(data), data_len, ts, ntp_ts, rtp_flags);
}

/* Disabled for now
rtp_error_t uvgrtp::media_stream::push_user_packet(uint8_t* data, uint32_t len)
{
    return impl_->push_user_packet(data, len);
}

rtp_error_t uvgrtp::media_stream::install_user_receive_hook(void* arg, void (*hook)(void*, uint8_t* payload, uint32_t len))
{
    return impl_->install_user_receive_hook(arg, hook);
}*/

uvgrtp::frame::rtp_frame *uvgrtp::media_stream::pull_frame()
{
    return impl_->pull_frame();
}

uvgrtp::frame::rtp_frame *uvgrtp::media_stream::pull_frame(size_t timeout_ms)
{
    return impl_->pull_frame(timeout_ms);

}

rtp_error_t uvgrtp::media_stream::install_receive_hook(void *arg, void (*hook)(void *, uvgrtp::frame::rtp_frame *))
{
    return impl_->install_receive_hook(arg, hook);
}

rtp_error_t uvgrtp::media_stream::configure_ctx(int rcc_flag, ssize_t value)
{
    return impl_->configure_ctx(rcc_flag, value);
}

int uvgrtp::media_stream::get_configuration_value(int rcc_flag)
{
    return impl_->get_configuration_value(rcc_flag);
}

uvgrtp::rtcp *uvgrtp::media_stream::get_rtcp()
{
    return impl_->get_rtcp();
}

uint32_t uvgrtp::media_stream::get_ssrc() const
{
    return impl_->get_ssrc();
}
