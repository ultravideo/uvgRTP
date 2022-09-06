#include "uvgrtp/media_stream.hh"

#include "uvgrtp/rtcp.hh"
#include "uvgrtp/socket.hh"

#include "formats/h264.hh"
#include "formats/h265.hh"
#include "formats/h266.hh"
#include "debug.hh"
#include "random.hh"
#include "rtp.hh"
#include "zrtp.hh"

#include "holepuncher.hh"
#include "reception_flow.hh"
#include "srtp/srtcp.hh"
#include "srtp/srtp.hh"
#include "formats/media.hh"

#include <cstring>
#include <errno.h>

uvgrtp::media_stream::media_stream(std::string cname, std::string remote_addr, 
    std::string local_addr, uint16_t src_port, uint16_t dst_port, rtp_format_t fmt, 
    int rce_flags):
    key_(uvgrtp::random::generate_32()),
    srtp_(nullptr),
    srtcp_(nullptr),
    socket_(nullptr),
    rtp_(nullptr),
    rtcp_(nullptr),
    remote_sockaddr_(),
    remote_address_(remote_addr),
    local_address_(local_addr),
    src_port_(src_port),
    dst_port_(dst_port),
    fmt_(fmt),
    rce_flags_(rce_flags),
    media_config_(nullptr),
    initialized_(false),
    rtp_handler_key_(0),
    zrtp_handler_key_(0),
    reception_flow_(nullptr),
    media_(nullptr),
    holepuncher_(nullptr),
    cname_(cname),
    fps_enumerator_(30),
    fps_denominator_(1)
{}

uvgrtp::media_stream::~media_stream()
{
    if (reception_flow_)
    {
        reception_flow_->stop();
    }

    // TODO: I would take a close look at what happens when pull_frame is called
    // and media stream is destroyed. Note that this is the only way to stop pull
    // frame without waiting

    if ((rce_flags_ & RCE_RTCP) && rtcp_)
    {
        rtcp_->stop();
    }

    if ((rce_flags_ & RCE_HOLEPUNCH_KEEPALIVE) && holepuncher_)
    {
        holepuncher_->stop();
    }

    (void)free_resources(RTP_OK);
}

rtp_error_t uvgrtp::media_stream::init_connection()
{
    rtp_error_t ret = RTP_OK;

    socket_ = std::shared_ptr<uvgrtp::socket> (new uvgrtp::socket(rce_flags_));

    if ((ret = socket_->init(AF_INET, SOCK_DGRAM, 0)) != RTP_OK)
        return ret;

#ifdef _WIN32
    /* Make the socket non-blocking */
    int enabled = 1;

    if (::ioctlsocket(socket_->get_raw_socket(), FIONBIO, (u_long *)&enabled) < 0)
        UVG_LOG_ERROR("Failed to make the socket non-blocking!");
#endif

    short int family = AF_INET;

    if (!(rce_flags_ & RCE_RECEIVE_ONLY) && remote_address_ != "")
    {
        // no reason to fail sending even if binding fails so we set remote address first
        remote_sockaddr_ = socket_->create_sockaddr(family, remote_address_, dst_port_);
        socket_->set_sockaddr(remote_sockaddr_);
    }
    else
    {
        UVG_LOG_INFO("Sending disabled for this stream");
    }

    if (!(rce_flags_ & RCE_SEND_ONLY))
    {
        if (local_address_ != "" && src_port_ != 0) {
            sockaddr_in bind_addr = socket_->create_sockaddr(family, local_address_, src_port_);

            if ((ret = socket_->bind(bind_addr)) != RTP_OK)
            {
                log_platform_error("bind(2) failed");
                return ret;
            }
        }
        else if (src_port_ != 0)
        {
            if ((ret = socket_->bind(family, INADDR_ANY, src_port_)) != RTP_OK)
            {
                log_platform_error("bind(2) to any failed");
                return ret;
            }
        }
        else
        {
            UVG_LOG_INFO("Not binding, receiving is not possible");
        }
    }
    else
    {
        UVG_LOG_INFO("Not binding, receiving is not possible");
    }

    /* Set the default UDP send/recv buffer sizes to 4MB as on Windows
     * the default size is way too small for a larger video conference */
    int buf_size = 4 * 1024 * 1024;

    if ((ret = socket_->setsockopt(SOL_SOCKET, SO_SNDBUF, (const char*)&buf_size, sizeof(int))) != RTP_OK)
    {
        return ret;
    }

    if ((ret = socket_->setsockopt(SOL_SOCKET, SO_RCVBUF, (const char*)&buf_size, sizeof(int))) != RTP_OK)
    {
        return ret;
    }

    return ret;
}

rtp_error_t uvgrtp::media_stream::create_media(rtp_format_t fmt)
{
    switch (fmt) {
        case RTP_FORMAT_H264:
        {
            uvgrtp::formats::h264* format_264 = new uvgrtp::formats::h264(socket_, rtp_, rce_flags_);

            reception_flow_->install_aux_handler_cpp(
                rtp_handler_key_,
                std::bind(&uvgrtp::formats::h264::packet_handler, format_264, std::placeholders::_1, std::placeholders::_2),
                std::bind(&uvgrtp::formats::h264::frame_getter, format_264, std::placeholders::_1));
            media_.reset(format_264);

            return RTP_OK;
            break;
        }
        case RTP_FORMAT_H265:
        {
            uvgrtp::formats::h265* format_265 = new uvgrtp::formats::h265(socket_, rtp_, rce_flags_);

            reception_flow_->install_aux_handler_cpp(
                rtp_handler_key_,
                std::bind(&uvgrtp::formats::h265::packet_handler, format_265, std::placeholders::_1, std::placeholders::_2),
                std::bind(&uvgrtp::formats::h265::frame_getter, format_265, std::placeholders::_1));
            media_.reset(format_265);

            return RTP_OK;
            break;
        }
        case RTP_FORMAT_H266:
        {
            uvgrtp::formats::h266* format_266 = new uvgrtp::formats::h266(socket_, rtp_, rce_flags_);

            reception_flow_->install_aux_handler_cpp(
                rtp_handler_key_,
                std::bind(&uvgrtp::formats::h266::packet_handler, format_266, std::placeholders::_1, std::placeholders::_2),
                std::bind(&uvgrtp::formats::h266::frame_getter, format_266, std::placeholders::_1));
            media_.reset(format_266);

            return RTP_OK;
            break;
        }
        case RTP_FORMAT_OPUS:
        case RTP_FORMAT_PCMU:
        case RTP_FORMAT_GSM:
        case RTP_FORMAT_G723:
        case RTP_FORMAT_DVI4_32:
        case RTP_FORMAT_DVI4_64:
        case RTP_FORMAT_LPC:
        case RTP_FORMAT_PCMA:
        case RTP_FORMAT_G722:
        case RTP_FORMAT_L16_STEREO:
        case RTP_FORMAT_L16_MONO:
        case RTP_FORMAT_G728:
        case RTP_FORMAT_DVI4_441:
        case RTP_FORMAT_DVI4_882:
        case RTP_FORMAT_G729:
        case RTP_FORMAT_G726_40:
        case RTP_FORMAT_G726_32:
        case RTP_FORMAT_G726_24:
        case RTP_FORMAT_G726_16:
        case RTP_FORMAT_G729D:
        case RTP_FORMAT_G729E:
        case RTP_FORMAT_GSM_EFR:
        case RTP_FORMAT_L8:
        case RTP_FORMAT_VDVI:
            media_ = std::unique_ptr<uvgrtp::formats::media> (new uvgrtp::formats::media(socket_, rtp_, rce_flags_));

            reception_flow_->install_aux_handler(
                rtp_handler_key_,
                media_->get_media_frame_info(),
                media_->packet_handler,
                nullptr
            );
            return RTP_OK;

        default:
            UVG_LOG_ERROR("Unknown payload format %u\n", fmt_);
            media_ = nullptr;
            return RTP_NOT_SUPPORTED;
    }
}

rtp_error_t uvgrtp::media_stream::free_resources(rtp_error_t ret)
{
    if (rtcp_)
    {
        rtcp_ = nullptr;
    }
    if (rtp_)
    {
        rtp_ = nullptr;
    }
    if (srtp_)
    {
        srtp_ = nullptr;
    }
    if (srtcp_)
    {
        srtcp_ = nullptr;
    }
    if (reception_flow_)
    {
        reception_flow_ = nullptr;
    }
    if (holepuncher_)
    {
        holepuncher_ = nullptr;
    }
    if (media_)
    {
        media_ = nullptr;
    }
    if (socket_)
    {
        socket_ = nullptr;
    }

    return ret;
}

rtp_error_t uvgrtp::media_stream::init()
{
    if (init_connection() != RTP_OK) {
        UVG_LOG_ERROR("Failed to initialize the underlying socket");
        return free_resources(RTP_GENERIC_ERROR);
    }

    reception_flow_ = std::unique_ptr<uvgrtp::reception_flow> (new uvgrtp::reception_flow());

    rtp_ = std::shared_ptr<uvgrtp::rtp> (new uvgrtp::rtp(fmt_));
    rtcp_ = std::shared_ptr<uvgrtp::rtcp> (new uvgrtp::rtcp(rtp_, cname_, rce_flags_));

    socket_->install_handler(rtcp_.get(), rtcp_->send_packet_handler_vec);

    rtp_handler_key_ = reception_flow_->install_handler(rtp_->packet_handler);
    reception_flow_->install_aux_handler(rtp_handler_key_, rtcp_.get(), rtcp_->recv_packet_handler, nullptr);

    return start_components();
}

rtp_error_t uvgrtp::media_stream::init(std::shared_ptr<uvgrtp::zrtp> zrtp)
{
    if (init_connection() != RTP_OK) {
        log_platform_error("Failed to initialize the underlying socket");
        return RTP_GENERIC_ERROR;
    }

    reception_flow_ = std::unique_ptr<uvgrtp::reception_flow> (new uvgrtp::reception_flow());

    rtp_ = std::shared_ptr<uvgrtp::rtp> (new uvgrtp::rtp(fmt_));

    bool perform_dh = !(rce_flags_ & RCE_ZRTP_MULTISTREAM_NO_DH);
    if (!perform_dh)
    {
        UVG_LOG_DEBUG("Sleeping non-DH performing stream until DH has finished");
        std::chrono::system_clock::time_point tp = std::chrono::system_clock::now();

        while (!zrtp->has_dh_finished())
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));

            if (std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now() - tp).count() > 10)
            {
                UVG_LOG_ERROR("Giving up on DH after 10 seconds");
                return free_resources(RTP_TIMEOUT);
            }
        }
    }

    rtp_error_t ret = RTP_OK;
    if ((ret = zrtp->init(rtp_->get_ssrc(), socket_, remote_sockaddr_, perform_dh)) != RTP_OK) {
        UVG_LOG_WARN("Failed to initialize ZRTP for media stream!");
        return free_resources(ret);
    }

    srtp_ = std::shared_ptr<uvgrtp::srtp>(new uvgrtp::srtp(rce_flags_));
    if ((ret = init_srtp_with_zrtp(rce_flags_, SRTP, srtp_, zrtp)) != RTP_OK)
      return free_resources(ret);

    srtcp_ = std::shared_ptr<uvgrtp::srtcp> (new uvgrtp::srtcp());
    if ((ret = init_srtp_with_zrtp(rce_flags_, SRTCP, srtcp_, zrtp)) != RTP_OK)
      return free_resources(ret);

    zrtp->dh_has_finished(); // only after the DH stream has gotten its keys, do we let non-DH stream perform ZRTP

    rtcp_ = std::shared_ptr<uvgrtp::rtcp> (new uvgrtp::rtcp(rtp_, cname_, srtcp_, rce_flags_));

    socket_->install_handler(rtcp_.get(), rtcp_->send_packet_handler_vec);
    socket_->install_handler(srtp_.get(), srtp_->send_packet_handler);

    rtp_handler_key_  = reception_flow_->install_handler(rtp_->packet_handler);
    zrtp_handler_key_ = reception_flow_->install_handler(zrtp->packet_handler);

    reception_flow_->install_aux_handler(rtp_handler_key_, rtcp_.get(), rtcp_->recv_packet_handler, nullptr);
    reception_flow_->install_aux_handler(rtp_handler_key_, srtp_.get(), srtp_->recv_packet_handler, nullptr);

    return start_components();
}

rtp_error_t uvgrtp::media_stream::add_srtp_ctx(uint8_t *key, uint8_t *salt)
{
    if (!key || !salt)
        return RTP_INVALID_VALUE;

    unsigned int srtp_rce_flags = RCE_SRTP | RCE_SRTP_KMNGMNT_USER;
    rtp_error_t ret     = RTP_OK;

    if (init_connection() != RTP_OK) {
        UVG_LOG_ERROR("Failed to initialize the underlying socket");
        return free_resources(RTP_GENERIC_ERROR);
    }

    if ((rce_flags_ & srtp_rce_flags) != srtp_rce_flags)
        return free_resources(RTP_NOT_SUPPORTED);

    reception_flow_ = std::unique_ptr<uvgrtp::reception_flow> (new uvgrtp::reception_flow());

    rtp_ = std::shared_ptr<uvgrtp::rtp> (new uvgrtp::rtp(fmt_));

    srtp_ = std::shared_ptr<uvgrtp::srtp> (new uvgrtp::srtp(rce_flags_));

    // why are they local and remote key/salt the same?
    if ((ret = srtp_->init(SRTP, rce_flags_, key, key, salt, salt)) != RTP_OK) {
        UVG_LOG_WARN("Failed to initialize SRTP for media stream!");
        return free_resources(ret);
    }

    srtcp_ = std::shared_ptr<uvgrtp::srtcp> (new uvgrtp::srtcp());

    if ((ret = srtcp_->init(SRTCP, rce_flags_, key, key, salt, salt)) != RTP_OK) {
        UVG_LOG_WARN("Failed to initialize SRTCP for media stream!");
        return free_resources(ret);
    }

    rtcp_ = std::shared_ptr<uvgrtp::rtcp> (new uvgrtp::rtcp(rtp_, cname_, srtcp_, rce_flags_));

    socket_->install_handler(rtcp_.get(), rtcp_->send_packet_handler_vec);
    socket_->install_handler(srtp_.get(), srtp_->send_packet_handler);

    rtp_handler_key_ = reception_flow_->install_handler(rtp_->packet_handler);

    reception_flow_->install_aux_handler(rtp_handler_key_, rtcp_.get(), rtcp_->recv_packet_handler, nullptr);
    reception_flow_->install_aux_handler(rtp_handler_key_, srtp_.get(), srtp_->recv_packet_handler, nullptr);

    return start_components();
}

rtp_error_t uvgrtp::media_stream::start_components()
{
    if (create_media(fmt_) != RTP_OK)
        return free_resources(RTP_MEMORY_ERROR);

    if (rce_flags_ & RCE_HOLEPUNCH_KEEPALIVE) {
        holepuncher_ = std::unique_ptr<uvgrtp::holepuncher> (new uvgrtp::holepuncher(socket_));
        holepuncher_->start();
    }

    if (rce_flags_ & RCE_RTCP) {

        if (remote_address_ == "" ||
            src_port_ == 0 ||
            dst_port_ == 0)
        {
            UVG_LOG_ERROR("Using RTCP requires setting at least remote address, local port and remote port");
        }
        else
        {
            rtcp_->add_participant(local_address_, remote_address_, src_port_ + 1, dst_port_ + 1, rtp_->get_clock_rate());
            rtcp_->set_session_bandwidth(get_default_bandwidth_kbps(fmt_));
            rtcp_->start();
        }
    }

    if (rce_flags_ & RCE_SRTP_AUTHENTICATE_RTP)
        rtp_->set_payload_size(MAX_PAYLOAD - UVG_AUTH_TAG_LENGTH);

    initialized_ = true;
    return reception_flow_->start(socket_, rce_flags_);
}

rtp_error_t uvgrtp::media_stream::push_frame(uint8_t *data, size_t data_len, int rtp_flags)
{
    rtp_error_t ret = check_push_preconditions();
    if (ret == RTP_OK)
    {
        if (rce_flags_ & RCE_HOLEPUNCH_KEEPALIVE && holepuncher_)
            holepuncher_->notify();

        ret = media_->push_frame(data, data_len, rtp_flags);
    }

    return ret;
}

rtp_error_t uvgrtp::media_stream::push_frame(std::unique_ptr<uint8_t[]> data, size_t data_len, int rtp_flags)
{
    rtp_error_t ret = check_push_preconditions();
    if (ret == RTP_OK)
    {
        if (rce_flags_ & RCE_HOLEPUNCH_KEEPALIVE && holepuncher_)
            holepuncher_->notify();

        ret = media_->push_frame(std::move(data), data_len, rtp_flags);
    }

    return ret;
}

rtp_error_t uvgrtp::media_stream::push_frame(uint8_t *data, size_t data_len, uint32_t ts, int rtp_flags)
{
    rtp_error_t ret = check_push_preconditions();
    if (ret == RTP_OK)
    {
        if (rce_flags_ & RCE_HOLEPUNCH_KEEPALIVE)
            holepuncher_->notify();

        rtp_->set_timestamp(ts);
        ret = media_->push_frame(data, data_len, rtp_flags);
        rtp_->set_timestamp(INVALID_TS);
    }

    return ret;
}

rtp_error_t uvgrtp::media_stream::push_frame(std::unique_ptr<uint8_t[]> data, size_t data_len, uint32_t ts, int rtp_flags)
{
    rtp_error_t ret = check_push_preconditions();
    if (ret == RTP_OK)
    {
        if (rce_flags_ & RCE_HOLEPUNCH_KEEPALIVE)
            holepuncher_->notify();

        rtp_->set_timestamp(ts);
        ret = media_->push_frame(std::move(data), data_len, rtp_flags);
        rtp_->set_timestamp(INVALID_TS);
    }

    return ret;
}

uvgrtp::frame::rtp_frame *uvgrtp::media_stream::pull_frame()
{
    if (!check_pull_preconditions()) {
        return nullptr;
    }

    return reception_flow_->pull_frame();
}

uvgrtp::frame::rtp_frame *uvgrtp::media_stream::pull_frame(size_t timeout_ms)
{
    if (!check_pull_preconditions()) {
        return nullptr;
    }

    return reception_flow_->pull_frame(timeout_ms);
}

bool uvgrtp::media_stream::check_pull_preconditions()
{
    if (!initialized_) {
        UVG_LOG_ERROR("RTP context has not been initialized fully, cannot continue!");
        rtp_errno = RTP_NOT_INITIALIZED;
        return false;
    }

    return true;
}

rtp_error_t uvgrtp::media_stream::check_push_preconditions()
{
    if (!initialized_) {
        UVG_LOG_ERROR("RTP context has not been initialized fully, cannot continue!");
        return RTP_NOT_INITIALIZED;
    }

    if (remote_address_ == "")
    {
        UVG_LOG_ERROR("Cannot push frame if remote address has not been provided!");
        return RTP_INVALID_VALUE;
    }

    return RTP_OK;
}

rtp_error_t uvgrtp::media_stream::install_receive_hook(void *arg, void (*hook)(void *, uvgrtp::frame::rtp_frame *))
{
    if (!initialized_) {
        UVG_LOG_ERROR("RTP context has not been initialized fully, cannot continue!");
        return RTP_NOT_INITIALIZED;
    }

    if (!hook)
        return RTP_INVALID_VALUE;

    reception_flow_->install_receive_hook(arg, hook);

    return RTP_OK;
}

void uvgrtp::media_stream::set_media_config(void *config)
{
    media_config_ = config;
}

void *uvgrtp::media_stream::get_media_config()
{
    return media_config_;
}

rtp_error_t uvgrtp::media_stream::configure_ctx(int rcc_flag, ssize_t value)
{
    if (!initialized_) {
        UVG_LOG_ERROR("RTP context has not been initialized fully, cannot continue!");
        return RTP_NOT_INITIALIZED;
    }

    rtp_error_t ret = RTP_OK;

    switch (rcc_flag) {
        case RCC_UDP_SND_BUF_SIZE: {
            if (value <= 0)
                return RTP_INVALID_VALUE;

            int buf_size = (int)value;
            if ((ret = socket_->setsockopt(SOL_SOCKET, SO_SNDBUF, (const char *)&buf_size, sizeof(int))) != RTP_OK)
                return ret;
        }
        break;

        case RCC_UDP_RCV_BUF_SIZE: {
            if (value <= 0)
                return RTP_INVALID_VALUE;

            reception_flow_->set_buffer_size(value);

            int buf_size = (int)value;
            if ((ret = socket_->setsockopt(SOL_SOCKET, SO_RCVBUF, (const char *)&buf_size, sizeof(int))) != RTP_OK)
                return ret;
        }
        break;

        case RCC_PKT_MAX_DELAY: {
            if (value <= 0)
                return RTP_INVALID_VALUE;

            rtp_->set_pkt_max_delay(value);
        }
        break;

        case RCC_DYN_PAYLOAD_TYPE: {
            if (value <= 0 || UINT8_MAX < value)
                return RTP_INVALID_VALUE;

            rtp_->set_dynamic_payload((uint8_t)value);
        }
        break;

        case RCC_MTU_SIZE: {
            ssize_t hdr      = ETH_HDR_SIZE + IPV4_HDR_SIZE + UDP_HDR_SIZE + RTP_HDR_SIZE;
            ssize_t max_size = 0xffff - IPV4_HDR_SIZE - UDP_HDR_SIZE;

            if (rce_flags_ & RCE_SRTP_AUTHENTICATE_RTP)
                hdr += UVG_AUTH_TAG_LENGTH;

            if (value <= hdr)
                return RTP_INVALID_VALUE;


            if (value > max_size) {
                unsigned int u_max_size = (unsigned int)max_size;
                UVG_LOG_ERROR("Payload size (%zd) is larger than maximum UDP datagram size (%u)",
                        value, u_max_size);
                return RTP_INVALID_VALUE;
            }

            rtp_->set_payload_size(value - hdr);
            rtcp_->set_mtu_size(value - (ETH_HDR_SIZE + IPV4_HDR_SIZE + UDP_HDR_SIZE));

        }
        break;

        case RCC_FPS_ENUMERATOR: {
            fps_enumerator_ = value;

            media_->set_fps(fps_enumerator_, fps_denominator_);
            break;
        }

        case RCC_FPS_DENOMINATOR: {
            fps_denominator_ = value;

            media_->set_fps(fps_enumerator_, fps_denominator_);
            break;
        }           

        default:
            return RTP_INVALID_VALUE;
    }

    return ret;
}

uint32_t uvgrtp::media_stream::get_key() const
{
    return key_;
}

uvgrtp::rtcp *uvgrtp::media_stream::get_rtcp()
{
    return rtcp_.get();
}

uint32_t uvgrtp::media_stream::get_ssrc() const
{
    if (!initialized_ || rtp_ == nullptr) {
        UVG_LOG_ERROR("RTP context has not been initialized, please call init before asking ssrc!");
        return 0;
    }

    return rtp_->get_ssrc();
}

rtp_error_t uvgrtp::media_stream::init_srtp_with_zrtp(int rce_flags, int type, std::shared_ptr<uvgrtp::base_srtp> srtp,
    std::shared_ptr<uvgrtp::zrtp> zrtp)
{
    uint32_t key_size = srtp->get_key_size(rce_flags);

    uint8_t* local_key = new uint8_t[key_size];
    uint8_t* remote_key = new uint8_t[key_size];
    uint8_t local_salt[UVG_SALT_LENGTH];
    uint8_t remote_salt[UVG_SALT_LENGTH];

    rtp_error_t ret = zrtp->get_srtp_keys(
        local_key,   key_size * 8,
        remote_key,  key_size * 8,
        local_salt,  UVG_SALT_LENGTH * 8,
        remote_salt, UVG_SALT_LENGTH * 8
     );

    if (ret == RTP_OK)
    {
        ret = srtp->init(type, rce_flags, local_key, remote_key,
                        local_salt, remote_salt);
    }
    else
    {
        UVG_LOG_WARN("Failed to initialize SRTP for media stream!");
    }

    delete[] local_key;
    delete[] remote_key;

    return ret;
}


uint32_t uvgrtp::media_stream::get_default_bandwidth_kbps(rtp_format_t fmt)
{
    int bandwidth = 50;
    switch (fmt) {
        case RTP_FORMAT_PCMU:
        case RTP_FORMAT_PCMA:
        case RTP_FORMAT_L8: // L8 bitrate depends on sampling rate
            bandwidth = 64;
            break;
        case RTP_FORMAT_G723:
            bandwidth = 6;
            break;
        case RTP_FORMAT_DVI4_32:
            bandwidth = 32;
            break;
        case RTP_FORMAT_DVI4_64:
            bandwidth = 64;
            break;
        case RTP_FORMAT_LPC:
            bandwidth = 6;
            break;
        case RTP_FORMAT_GSM:
            bandwidth = 13;
            break;
        case RTP_FORMAT_G722:
            bandwidth = 64;
            break;
        case RTP_FORMAT_L16_STEREO:
            bandwidth = 1411;
            break;
        case RTP_FORMAT_L16_MONO:
            bandwidth = 706;
            break;
        case RTP_FORMAT_G728:
            bandwidth = 16;
            break;
        case RTP_FORMAT_DVI4_441:
            bandwidth = 44;
            break;
        case RTP_FORMAT_DVI4_882:
            bandwidth = 88;
            break;
        case RTP_FORMAT_G729:
            bandwidth = 8;
            break;
        case RTP_FORMAT_G726_40:
            bandwidth = 40;
            break;
        case RTP_FORMAT_G726_32:
            bandwidth = 32;
            break;
        case RTP_FORMAT_G726_24:
            bandwidth = 24;
            break;
        case RTP_FORMAT_G726_16:
            bandwidth = 16;
            break;
        case RTP_FORMAT_G729D:
            bandwidth = 6;
            break;
        case RTP_FORMAT_G729E:
            bandwidth = 11;
            break;
        case RTP_FORMAT_GSM_EFR:
            bandwidth = 12;
            break;
        case RTP_FORMAT_VDVI:
            bandwidth = 25;
            break;
        case RTP_FORMAT_H264:
            bandwidth = 6000;
            break;
        case RTP_FORMAT_H265:
            bandwidth = 3000;
            break;
        case RTP_FORMAT_H266:
            bandwidth = 2000;
            break;
        case RTP_FORMAT_OPUS:
            bandwidth = 24;
            break;
        default:
            UVG_LOG_WARN("Unknown RTP format, setting session bandwidth to 64 kbps");
            bandwidth = 64;
            break;
    }

    return bandwidth;
}
