#include "uvgrtp/rtcp.hh"

#include "rtcp_internal.hh"


uvgrtp::rtcp::rtcp(std::shared_ptr<uvgrtp::rtp> rtp,
    std::shared_ptr<std::atomic_uint> ssrc,
    std::shared_ptr<std::atomic<uint32_t>> remote_ssrc,
    std::string cname,
    std::shared_ptr<uvgrtp::socketfactory> sfp,
    int rce_flags)
    : pimpl_(std::make_shared<rtcp_internal>(rtp, ssrc, remote_ssrc, std::move(cname), sfp, nullptr, rce_flags))
{}

uvgrtp::rtcp::rtcp(std::shared_ptr<uvgrtp::rtp> rtp,
    std::shared_ptr<std::atomic_uint> ssrc,
    std::shared_ptr<std::atomic<uint32_t>> remote_ssrc,
    std::string cname,
    std::shared_ptr<uvgrtp::socketfactory> sfp,
    std::shared_ptr<uvgrtp::srtcp> srtcp,
    int rce_flags)
    : pimpl_(std::make_shared<rtcp_internal>(rtp, ssrc, remote_ssrc, std::move(cname), sfp, srtcp, rce_flags))
{}

uvgrtp::rtcp::~rtcp()
{
}


rtp_error_t uvgrtp::rtcp::remove_all_hooks()
{
    return pimpl_->remove_all_hooks();
}

rtp_error_t uvgrtp::rtcp::remove_send_app_hook(std::string app_name)
{
    return pimpl_->remove_send_app_hook(std::move(app_name));
}

rtp_error_t uvgrtp::rtcp::install_sender_hook(void (*hook)(uvgrtp::frame::rtcp_sender_report*))
{
    return pimpl_->install_sender_hook(hook);
}

rtp_error_t uvgrtp::rtcp::install_sender_hook(std::function<void(std::shared_ptr<uvgrtp::frame::rtcp_sender_report>)> sr_handler)
{
    return pimpl_->install_sender_hook(std::move(sr_handler));
}

rtp_error_t uvgrtp::rtcp::install_sender_hook(std::function<void(std::unique_ptr<uvgrtp::frame::rtcp_sender_report>)> sr_handler)
{
    return pimpl_->install_sender_hook(std::move(sr_handler));
}

rtp_error_t uvgrtp::rtcp::install_receiver_hook(void (*hook)(uvgrtp::frame::rtcp_receiver_report*))
{
    return pimpl_->install_receiver_hook(hook);
}

rtp_error_t uvgrtp::rtcp::install_receiver_hook(std::function<void(std::shared_ptr<uvgrtp::frame::rtcp_receiver_report>)> rr_handler)
{
    return pimpl_->install_receiver_hook(std::move(rr_handler));
}

rtp_error_t uvgrtp::rtcp::install_receiver_hook(std::function<void(std::unique_ptr<uvgrtp::frame::rtcp_receiver_report>)> rr_handler)
{
    return pimpl_->install_receiver_hook(std::move(rr_handler));
}

rtp_error_t uvgrtp::rtcp::install_sdes_hook(void (*hook)(uvgrtp::frame::rtcp_sdes_packet*))
{
    return pimpl_->install_sdes_hook(hook);
}

rtp_error_t uvgrtp::rtcp::install_sdes_hook(std::function<void(std::shared_ptr<uvgrtp::frame::rtcp_sdes_packet>)> sdes_handler)
{
    return pimpl_->install_sdes_hook(std::move(sdes_handler));
}

rtp_error_t uvgrtp::rtcp::install_sdes_hook(std::function<void(std::unique_ptr<uvgrtp::frame::rtcp_sdes_packet>)> sdes_handler)
{
    return pimpl_->install_sdes_hook(std::move(sdes_handler));
}

rtp_error_t uvgrtp::rtcp::install_app_hook(void (*hook)(uvgrtp::frame::rtcp_app_packet*))
{
    return pimpl_->install_app_hook(hook);
}

rtp_error_t uvgrtp::rtcp::install_app_hook(std::function<void(std::shared_ptr<uvgrtp::frame::rtcp_app_packet>)> app_handler)
{
    return pimpl_->install_app_hook(std::move(app_handler));
}

rtp_error_t uvgrtp::rtcp::install_app_hook(std::function<void(std::unique_ptr<uvgrtp::frame::rtcp_app_packet>)> app_handler)
{
    return pimpl_->install_app_hook(std::move(app_handler));
}

rtp_error_t uvgrtp::rtcp::install_send_app_hook(std::string app_name,
    std::function<std::unique_ptr<uint8_t[]>(uint8_t& subtype, uint32_t& payload_len)> app_sending_func)
{
    return pimpl_->install_send_app_hook(std::move(app_name), std::move(app_sending_func));
}

uvgrtp::frame::rtcp_sender_report* uvgrtp::rtcp::get_sender_packet(uint32_t ssrc)
{
    return pimpl_->get_sender_packet(ssrc);
}

uvgrtp::frame::rtcp_receiver_report* uvgrtp::rtcp::get_receiver_packet(uint32_t ssrc)
{
    return pimpl_->get_receiver_packet(ssrc);
}

uvgrtp::frame::rtcp_sdes_packet* uvgrtp::rtcp::get_sdes_packet(uint32_t ssrc)
{
    return pimpl_->get_sdes_packet(ssrc);
}

uvgrtp::frame::rtcp_app_packet* uvgrtp::rtcp::get_app_packet(uint32_t ssrc)
{
    return pimpl_->get_app_packet(ssrc);
}

void uvgrtp::rtcp::set_ts_info(uint64_t clock_start, uint32_t clock_rate, uint32_t rtp_ts_start)
{
    pimpl_->set_ts_info(clock_start, clock_rate, rtp_ts_start);
}

rtp_error_t uvgrtp::rtcp::send_sdes_packet(const std::vector<uvgrtp::frame::rtcp_sdes_item>& items)
{
    return pimpl_->send_sdes_packet(items);
}

rtp_error_t uvgrtp::rtcp::send_bye_packet(std::vector<uint32_t> ssrcs)
{
    return pimpl_->send_bye_packet(ssrcs);
}

rtp_error_t uvgrtp::rtcp::send_app_packet(const char* name, uint8_t subtype,
    uint32_t payload_len, const uint8_t* payload)
{
    return pimpl_->send_app_packet(name, subtype, payload_len, payload);
}