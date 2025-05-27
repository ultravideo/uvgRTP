#include "uvgrtp/rtcp.hh"

#include "rtcp_internal.hh"


uvgrtp::frame::rtcp_sr convert_sr(const uvgrtp::frame::rtcp_sender_report* sr)
{
    uvgrtp::frame::rtcp_sr converted;
    converted.header = sr->header;
    converted.ssrc = sr->ssrc;
    converted.sender_info = sr->sender_info;

    // Allocate memory for report_blocks
    size_t block_count = sr->report_blocks.size();
    converted.report_blocks = new uvgrtp::frame::rtcp_report_block[block_count];

    // Copy the report blocks into the new allocated memory
    for (size_t i = 0; i < block_count; ++i) 
    {
        converted.report_blocks[i] = sr->report_blocks[i];
    }

    return converted;
}

uvgrtp::frame::rtcp_rr convert_rr(const uvgrtp::frame::rtcp_receiver_report* rr)
{
    uvgrtp::frame::rtcp_rr converted;
    converted.header = rr->header;
    converted.ssrc = rr->ssrc;

    // Allocate memory for report_blocks
    size_t block_count = rr->report_blocks.size();
    converted.report_blocks = new uvgrtp::frame::rtcp_report_block[block_count];

    // Copy the report blocks into the new allocated memory
    for (size_t i = 0; i < block_count; ++i) 
    {
        converted.report_blocks[i] = rr->report_blocks[i];
    }

    return converted;
}

uvgrtp::frame::rtcp_sdes convert_sdes_packet(const uvgrtp::frame::rtcp_sdes_packet* packet)
{
    uvgrtp::frame::rtcp_sdes converted;
    converted.header = packet->header;

    // Use sc from the header to determine chunk count
    size_t chunk_count = packet->header.count;

    // Allocate memory for chunks
    converted.chunks = new uvgrtp::frame::rtcp_sdes_ck[chunk_count];

    for (size_t i = 0; i < chunk_count; ++i) {
        const auto& chunk = packet->chunks[i];

        converted.chunks[i].ssrc = chunk.ssrc;

        // Allocate memory for items and set item_count
        converted.chunks[i].item_count = chunk.items.size();
        converted.chunks[i].items = new uvgrtp::frame::rtcp_sdes_item[converted.chunks[i].item_count];

        // Copy items
        for (size_t j = 0; j < converted.chunks[i].item_count; ++j) {
            converted.chunks[i].items[j] = chunk.items[j];
        }
    }

    return converted;
}

uvgrtp::rtcp::rtcp(std::shared_ptr<uvgrtp::rtp> rtp,
    std::shared_ptr<std::atomic_uint> ssrc,
    std::shared_ptr<std::atomic<uint32_t>> remote_ssrc,
    std::string cname,
    std::shared_ptr<uvgrtp::socketfactory> sfp,
    int rce_flags)
    : pimpl_(new rtcp_internal(rtp, ssrc, remote_ssrc, std::move(cname), sfp, nullptr, rce_flags))
{}

uvgrtp::rtcp::rtcp(std::shared_ptr<uvgrtp::rtp> rtp,
    std::shared_ptr<std::atomic_uint> ssrc,
    std::shared_ptr<std::atomic<uint32_t>> remote_ssrc,
    std::string cname,
    std::shared_ptr<uvgrtp::socketfactory> sfp,
    std::shared_ptr<uvgrtp::srtcp> srtcp,
    int rce_flags)
    : pimpl_(new rtcp_internal(rtp, ssrc, remote_ssrc, std::move(cname), sfp, srtcp, rce_flags))
{}

uvgrtp::rtcp::~rtcp()
{
    delete pimpl_;
}

rtp_error_t uvgrtp::rtcp::remove_all_hooks()
{
    return pimpl_->remove_all_hooks();
}



rtp_error_t uvgrtp::rtcp::install_sender_hook(void (*handler)(uvgrtp::frame::rtcp_sr*))
{
    if (!handler)
        return RTP_INVALID_VALUE;

    // instead add a function that also converts the type on the fly
    return pimpl_->install_sender_hook([handler](std::unique_ptr<uvgrtp::frame::rtcp_sender_report> sr) {
        uvgrtp::frame::rtcp_sr converted_sr = convert_sr(sr.get());
        handler(&converted_sr);
        });
}

rtp_error_t uvgrtp::rtcp::install_receiver_hook(void (*handler)(uvgrtp::frame::rtcp_rr*))
{
    if (!handler)
        return RTP_INVALID_VALUE;

    return pimpl_->install_receiver_hook([handler](std::unique_ptr<uvgrtp::frame::rtcp_receiver_report> rr) {
        uvgrtp::frame::rtcp_rr converted_rr = convert_rr(rr.get());
        handler(&converted_rr);
        });
}

rtp_error_t uvgrtp::rtcp::install_sdes_hook(void (*handler)(uvgrtp::frame::rtcp_sdes*))
{
    if (!handler)
        return RTP_INVALID_VALUE;

    return pimpl_->install_sdes_hook([handler](std::unique_ptr<uvgrtp::frame::rtcp_sdes_packet> sdes_packet) {
        uvgrtp::frame::rtcp_sdes converted_sdes = convert_sdes_packet(sdes_packet.get());
        handler(&converted_sdes);
        });
}

rtp_error_t uvgrtp::rtcp::install_send_app_hook(
    const char* app_name,
    uint8_t* (*send_hook)(uint8_t* subtype, uint32_t* payload_len, void* user_arg),
    void* user_arg
)
{
    if (!app_name || !send_hook)
        return RTP_INVALID_VALUE;

    // Convert app_name (const char*) to std::string
    std::string app_name_str(app_name);

    // Convert the C-style function pointer into a std::function
    auto app_sending_func = [send_hook](uint8_t& subtype, uint32_t& payload_len) -> std::unique_ptr<uint8_t[]> {
        return std::unique_ptr<uint8_t[]>(send_hook(&subtype, &payload_len, nullptr));
        };

    // Now call the internal function with the std::string and std::function
    return pimpl_->install_send_app_hook(app_name_str, app_sending_func);
}

rtp_error_t uvgrtp::rtcp::remove_send_app_hook(const char* app_name)
{
    if (!app_name)
        return RTP_INVALID_VALUE;

    // Convert app_name (const char*) to std::string
    std::string app_name_str(app_name);

    // Call internal remove function
    return pimpl_->remove_send_app_hook(app_name_str);
}

rtp_error_t uvgrtp::rtcp::send_bye_packet(const uint32_t* ssrcs, size_t count)
{
    if (ssrcs == nullptr || count == 0)
        return RTP_INVALID_VALUE;

    // Convert the raw array into a vector
    std::vector<uint32_t> ssrc_vector(ssrcs, ssrcs + count);

    // Call the internal function to send the BYE packet
    return pimpl_->send_bye_packet(ssrc_vector);
}

rtp_error_t uvgrtp::rtcp::add_sdes_item(const uvgrtp::frame::rtcp_sdes_item& item)
{
    return pimpl_->add_sdes_item(item);
}

rtp_error_t uvgrtp::rtcp::clear_sdes_items()
{
    return pimpl_->clear_sdes_items();
}

rtp_error_t uvgrtp::rtcp::send_app_packet(const char* name, uint8_t subtype,
    uint32_t payload_len, const uint8_t* payload)
{
    return pimpl_->send_app_packet(name, subtype, payload_len, payload);
}

uvgrtp::frame::rtcp_sr* uvgrtp::rtcp::get_sr(uint32_t ssrc)
{
    // Retrieve the internal sender report
    uvgrtp::frame::rtcp_sender_report* internal_sr = pimpl_->get_sender_packet(ssrc);

    if (!internal_sr)
        return nullptr;

    uvgrtp::frame::rtcp_sr* sr = new uvgrtp::frame::rtcp_sr();
    *sr = convert_sr(internal_sr);

    return sr;
}

uvgrtp::frame::rtcp_rr* uvgrtp::rtcp::get_rr(uint32_t ssrc)
{
    // Retrieve the internal receiver report
    uvgrtp::frame::rtcp_receiver_report* internal_rr = pimpl_->get_receiver_packet(ssrc);

    if (!internal_rr)
        return nullptr;

    uvgrtp::frame::rtcp_rr* rr = new uvgrtp::frame::rtcp_rr();
    *rr = convert_rr(internal_rr);

    return rr;
}

uvgrtp::frame::rtcp_sdes* uvgrtp::rtcp::get_sdes(uint32_t ssrc)
{
    // Retrieve the internal SDES packet
    uvgrtp::frame::rtcp_sdes_packet* internal_sdes = pimpl_->get_sdes_packet(ssrc);

    if (!internal_sdes)
        return nullptr;

    uvgrtp::frame::rtcp_sdes* sdes = new uvgrtp::frame::rtcp_sdes();
    *sdes = convert_sdes_packet(internal_sdes);

    return sdes;
}

void uvgrtp::rtcp::set_ts_info(uint64_t clock_start, uint32_t clock_rate, uint32_t rtp_ts_start)
{
    pimpl_->set_ts_info(clock_start, clock_rate, rtp_ts_start);
}

rtp_error_t uvgrtp::rtcp::install_app_hook(void (*hook)(uvgrtp::frame::rtcp_app_packet*))
{
    return pimpl_->install_app_hook(hook);
}

uvgrtp::frame::rtcp_app_packet* uvgrtp::rtcp::get_app_packet(uint32_t ssrc)
{
    return pimpl_->get_app_packet(ssrc);
}

#if UVGRTP_EXTENDED_API

rtp_error_t uvgrtp::rtcp::install_roundtrip_time_hook(std::function<void (uint32_t, uint32_t, double)> rtt_handler)
{
    if (!rtt_handler)
    {
        return RTP_INVALID_VALUE;
    }

    return pimpl_->install_roundtrip_time_hook([rtt_handler](uint32_t local_ssrc, uint32_t remote_ssrc, double rtt) { rtt_handler(local_ssrc, remote_ssrc, rtt); });
}

rtp_error_t uvgrtp::rtcp::remove_send_app_hook(const std::string& app_name)
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

rtp_error_t uvgrtp::rtcp::install_app_hook(std::function<void(std::shared_ptr<uvgrtp::frame::rtcp_app_packet>)> app_handler)
{
    return pimpl_->install_app_hook(std::move(app_handler));
}

rtp_error_t uvgrtp::rtcp::install_app_hook(std::function<void(std::unique_ptr<uvgrtp::frame::rtcp_app_packet>)> app_handler)
{
    return pimpl_->install_app_hook(std::move(app_handler));
}

rtp_error_t uvgrtp::rtcp::install_send_app_hook(const std::string& app_name,
    std::function<std::unique_ptr<uint8_t[]>(uint8_t& subtype, uint32_t& payload_len)> app_sending_func)
{
    return pimpl_->install_send_app_hook(app_name, std::move(app_sending_func));
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

rtp_error_t uvgrtp::rtcp::send_sdes_packet(const std::vector<uvgrtp::frame::rtcp_sdes_item>& items)
{
    rtp_error_t ret = pimpl_->clear_sdes_items();

    if (ret == RTP_OK)
    {
        for (auto item : items)
        {
            ret = pimpl_->add_sdes_item(item);
        }
    }

    return ret;
}

rtp_error_t uvgrtp::rtcp::send_bye_packet(const std::vector<uint32_t>& ssrcs)
{
    return pimpl_->send_bye_packet(ssrcs);
}

#endif
