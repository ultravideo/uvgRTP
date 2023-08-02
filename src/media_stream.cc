#include "uvgrtp/media_stream.hh"

#include "uvgrtp/rtcp.hh"

#include "formats/h264.hh"
#include "formats/h265.hh"
#include "formats/h266.hh"
#include "debug.hh"
#include "random.hh"
#include "rtp.hh"
#include "zrtp.hh"
#include "socket.hh"
#include "rtcp_reader.hh"

#include "holepuncher.hh"
#include "reception_flow.hh"
#include "srtp/srtcp.hh"
#include "srtp/srtp.hh"
#include "formats/media.hh"
#include "global.hh"
#include "socketfactory.hh"
#ifdef _WIN32
#include <Ws2tcpip.h>
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#endif

#include <cstring>
#include <errno.h>

uvgrtp::media_stream::media_stream(std::string cname, std::string remote_addr,
    std::string local_addr, uint16_t src_port, uint16_t dst_port, rtp_format_t fmt,
    std::shared_ptr<uvgrtp::socketfactory> sfp, int rce_flags) :
    key_(uvgrtp::random::generate_32()),
    srtp_(nullptr),
    srtcp_(nullptr),
    socket_(nullptr),
    rtp_(nullptr),
    rtcp_(nullptr),
    zrtp_(nullptr),
    sfp_(sfp),
    remote_sockaddr_(),
    remote_sockaddr_ip6_(),
    remote_address_(remote_addr),
    local_address_(local_addr),
    src_port_(src_port),
    dst_port_(dst_port),
    ipv6_(false),
    fmt_(fmt),
    new_socket_(false),
    rce_flags_(rce_flags),
    initialized_(false),
    reception_flow_(nullptr),
    media_(nullptr),
    holepuncher_(nullptr),
    cname_(cname),
    fps_numerator_(30),
    fps_denominator_(1),
    ssrc_(std::make_shared<std::atomic<std::uint32_t>>(uvgrtp::random::generate_32())),
    remote_ssrc_(std::make_shared<std::atomic<std::uint32_t>>(ssrc_.get()->load() + 1)),
    snd_buf_size_(-1),
    rcv_buf_size_(-1)
{
}

uvgrtp::media_stream::~media_stream()
{
    // TODO: I would take a close look at what happens when pull_frame is called
    // and media stream is destroyed. Note that this is the only way to stop pull
    // frame without waiting
    if (socket_) {
        socket_->remove_handler(ssrc_);
    }

    if ((rce_flags_ & RCE_RTCP) && rtcp_)
    {
        rtcp_->stop();
    }
    reception_flow_->remove_handlers(remote_ssrc_);
    // Clear this media stream from the reception_flow
    if ( reception_flow_ && (reception_flow_->clear_stream_from_flow(remote_ssrc_)) == 1) {
        reception_flow_->stop();
        if (sfp_) {
            sfp_->clear_port(src_port_, socket_);
        }
    }

    (void)free_resources(RTP_OK);
}

rtp_error_t uvgrtp::media_stream::init_connection()
{
    rtp_error_t ret = RTP_GENERIC_ERROR;
    ipv6_ = sfp_->get_ipv6();

    // First check if the given local address is a multicast address. If it is, the streams automatically gets its own socket,
    // regardless of any socket multiplexing measures
    sockaddr_in6 multicast_sockaddr6_;
    sockaddr_in multicast_sockaddr_;
    bool multicast = false;

    if (ipv6_ && src_port_ != 0 && local_address_ != "") {
        multicast_sockaddr6_ = uvgrtp::socket::create_ip6_sockaddr(local_address_, src_port_);
        if (uvgrtp::socket::is_multicast(multicast_sockaddr6_)) {
            socket_ = sfp_->create_new_socket(2, src_port_);
            new_socket_ = true;
            multicast = true;
        }
    }
    else if (src_port_ != 0 && local_address_ != "") {
        multicast_sockaddr_ = uvgrtp::socket::create_sockaddr(AF_INET, local_address_, src_port_);
        if (uvgrtp::socket::is_multicast(multicast_sockaddr_)) {
            socket_ = sfp_->create_new_socket(2, src_port_);
            new_socket_ = true;
            multicast = true;
        }
    }

    /* If the given local address is not a multicast address, get the socket */
    if (!multicast) {
        socket_ = sfp_->get_socket_ptr(2, src_port_);
        if (!socket_) {
            UVG_LOG_DEBUG("No socket found");
            return RTP_GENERIC_ERROR;
        }
    }


    //if (!(rce_flags_ & RCE_RECEIVE_ONLY) && remote_address_ != "" && dst_port_ != 0)
    if (remote_address_ != "" && dst_port_ != 0)
    {
        // no reason to fail sending even if binding fails so we set remote address first
        if (ipv6_) {
            remote_sockaddr_ip6_ = uvgrtp::socket::create_ip6_sockaddr(remote_address_, dst_port_);
        }
        else {
            remote_sockaddr_ = uvgrtp::socket::create_sockaddr(AF_INET, remote_address_, dst_port_);
        }
        holepuncher_ = std::unique_ptr<uvgrtp::holepuncher>(new uvgrtp::holepuncher(socket_));
        holepuncher_->set_remote_address(remote_sockaddr_, remote_sockaddr_ip6_);
    }
    if (rce_flags_ & RCE_RECEIVE_ONLY) {
        UVG_LOG_INFO("Sending disabled for this stream");
    }
    
    if (!(rce_flags_ & RCE_SEND_ONLY))
    {
        if (local_address_ == "" && src_port_ != 0)
        {
            if ((ret = sfp_->bind_socket_anyip(socket_, src_port_)) != RTP_OK)
            {
                log_platform_error("bind(2) to any failed");
                return ret;
            }
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
            reception_flow_->install_handler(
                5, remote_ssrc_,
                std::bind(&uvgrtp::formats::h264::packet_handler, format_264, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3,
                    std::placeholders::_4, std::placeholders::_5), nullptr
            );
            reception_flow_->install_getter(remote_ssrc_,
                std::bind(&uvgrtp::formats::h264::frame_getter, format_264, std::placeholders::_1));

            media_.reset(format_264);
            break;
        }
        case RTP_FORMAT_H265:
        {
            uvgrtp::formats::h265* format_265 = new uvgrtp::formats::h265(socket_, rtp_, rce_flags_);
            reception_flow_->install_handler(
                5, remote_ssrc_,
                std::bind(&uvgrtp::formats::h265::packet_handler, format_265, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3,
                    std::placeholders::_4, std::placeholders::_5), nullptr
            );
            reception_flow_->install_getter(remote_ssrc_,
                std::bind(&uvgrtp::formats::h265::frame_getter, format_265, std::placeholders::_1));

            media_.reset(format_265);
            break;
        }
        case RTP_FORMAT_H266:
        {
            uvgrtp::formats::h266* format_266 = new uvgrtp::formats::h266(socket_, rtp_, rce_flags_);
            reception_flow_->install_handler(
                5, remote_ssrc_,
                std::bind(&uvgrtp::formats::h266::packet_handler, format_266, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3,
                    std::placeholders::_4, std::placeholders::_5), nullptr
            );
            reception_flow_->install_getter(remote_ssrc_,
                std::bind(&uvgrtp::formats::h266::frame_getter, format_266, std::placeholders::_1));

            media_.reset(format_266);
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
        {
            media_ = std::unique_ptr<uvgrtp::formats::media>(new uvgrtp::formats::media(socket_, rtp_, rce_flags_));
            reception_flow_->install_handler(
                5, remote_ssrc_,
                std::bind(&uvgrtp::formats::media::packet_handler, media_.get(), std::placeholders::_1, std::placeholders::_2, std::placeholders::_3,
                    std::placeholders::_4, std::placeholders::_5), media_->get_media_frame_info());
            break;
        }
        default:
        {
            UVG_LOG_ERROR("Unknown payload format %u\n", fmt_);
            media_ = nullptr;
            return RTP_NOT_SUPPORTED;
        }
    }

    // set default values for fps
    media_->set_fps(fps_numerator_, fps_denominator_);
    return RTP_OK;
}

rtp_error_t uvgrtp::media_stream::free_resources(rtp_error_t ret)
{
    if ((rce_flags_ & RCE_HOLEPUNCH_KEEPALIVE) && holepuncher_)
    {
        holepuncher_->stop();
    }

    rtcp_           = nullptr;
    rtp_            = nullptr;
    srtp_           = nullptr;
    srtcp_          = nullptr;
    //reception_flow_ = nullptr;
    holepuncher_    = nullptr;
    media_          = nullptr;
    socket_         = nullptr;

    if (zrtp_) {
        zrtp_->set_zrtp_busy(false);
    }

    return ret;
}

rtp_error_t uvgrtp::media_stream::install_packet_handlers()
{
    reception_flow_->install_handler(
            1, remote_ssrc_,
            std::bind(&uvgrtp::rtp::packet_handler, rtp_, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3,
                std::placeholders::_4, std::placeholders::_5),
            nullptr);
    if (rce_flags_ & RCE_RTCP) {
            reception_flow_->install_handler(
                6, remote_ssrc_,
                std::bind(&uvgrtp::rtcp::recv_packet_handler_common, rtcp_, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3,
                std::placeholders::_4, std::placeholders::_5), rtcp_.get());
        }
    if (rce_flags_ & RCE_RTCP_MUX) {
            reception_flow_->install_handler(
                2, remote_ssrc_,
                std::bind(&uvgrtp::rtcp::handle_incoming_packet, rtcp_, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3,
                    std::placeholders::_4, std::placeholders::_5), nullptr);
        }
    if (rce_flags_ & RCE_SRTP) {
        socket_->install_handler(ssrc_, srtp_.get(), srtp_->send_packet_handler);
        reception_flow_->install_handler(
            4, remote_ssrc_,
            std::bind(&uvgrtp::srtp::recv_packet_handler, srtp_, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3,
                std::placeholders::_4, std::placeholders::_5), srtp_.get());
    }
    return RTP_OK;
}

rtp_error_t uvgrtp::media_stream::init(std::shared_ptr<uvgrtp::zrtp> zrtp)
{
    zrtp_ = zrtp;
    if (init_connection() != RTP_OK) {
        UVG_LOG_ERROR("Failed to initialize the underlying socket");
        return free_resources(RTP_GENERIC_ERROR);
    }

    reception_flow_ = sfp_->get_reception_flow_ptr(socket_);
    if (!reception_flow_) {
        UVG_LOG_ERROR("No reception flow found");
        return RTP_GENERIC_ERROR;
    }

    rtp_ = std::shared_ptr<uvgrtp::rtp>(new uvgrtp::rtp(fmt_, ssrc_, ipv6_));
    rtcp_ = std::shared_ptr<uvgrtp::rtcp>(new uvgrtp::rtcp(rtp_, ssrc_, remote_ssrc_, cname_, sfp_, rce_flags_));
    srtp_ = std::shared_ptr<uvgrtp::srtp>(new uvgrtp::srtp(rce_flags_));
    srtcp_ = std::shared_ptr<uvgrtp::srtcp>(new uvgrtp::srtcp());

    socket_->install_handler(ssrc_, rtcp_.get(), rtcp_->send_packet_handler_vec);

    /* If we are using ZRTP, we only install the ZRTP handler first. Rest of the handlers are installed after ZRTP is
       finished. If ZRTP is not enabled, we can install all the required handlers now */
    if ((rce_flags_ & RCE_ZRTP_DIFFIE_HELLMAN_MODE || rce_flags_ & RCE_ZRTP_MULTISTREAM_MODE
        || rce_flags_ & RCE_SRTP_KMNGMNT_ZRTP ) && zrtp_) {
        reception_flow_->install_handler(
            3, remote_ssrc_,
            std::bind(&uvgrtp::zrtp::packet_handler, zrtp_, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3,
                std::placeholders::_4, std::placeholders::_5),
            nullptr);
    }
    else {
        install_packet_handlers();
    }
    /* If we are using SRTP user keys, reception is started after SRTP is initalised in add_srtp_ctx() */
    if (rce_flags_ & RCE_SRTP_KMNGMNT_USER) {
        return RTP_OK;
    }
    return start_components();
}

rtp_error_t uvgrtp::media_stream::init_auto_zrtp(std::shared_ptr<uvgrtp::zrtp> zrtp)
{
    zrtp_ = zrtp;
    rtp_error_t ret = init(zrtp_);
    if (ret != RTP_OK) {
        UVG_LOG_ERROR("Failed to initialize media stream");
        return free_resources(ret);
    }
    ret = start_zrtp();
    return ret;
}

rtp_error_t uvgrtp::media_stream::start_zrtp()
{
    if (!zrtp_) {
        UVG_LOG_ERROR("ZRTP not found, stream %i", ssrc_.get()->load());
        return free_resources(RTP_GENERIC_ERROR);
    }
    bool perform_dh = !(rce_flags_ & RCE_ZRTP_MULTISTREAM_MODE);
    if (!perform_dh)
    {
        UVG_LOG_DEBUG("Sleeping non-DH performing stream until DH has finished");
        std::chrono::system_clock::time_point tp = std::chrono::system_clock::now();

        while (!zrtp_->has_dh_finished())
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));

            if (std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now() - tp).count() > 10)
            {
                UVG_LOG_ERROR("Giving up on DH after 10 seconds");
                return free_resources(RTP_TIMEOUT);
            }
        }
    }
    /* If ZRTP is already performing an MSM negotiation, wait for it to complete before starting a new one */
    if (!perform_dh) {
        auto start = std::chrono::system_clock::now();
        while (zrtp_->is_zrtp_busy()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            if (std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now() - start).count() > 10)
            {
                UVG_LOG_ERROR("Giving up on MSM after 10 seconds");
                return free_resources(RTP_TIMEOUT);
            }
        }
    }

    zrtp_->set_zrtp_busy(true);
    rtp_error_t ret = RTP_OK;
    if ((ret = zrtp_->init(rtp_->get_ssrc(), socket_, remote_sockaddr_, remote_sockaddr_ip6_, perform_dh, ipv6_)) != RTP_OK) {
        UVG_LOG_WARN("Failed to initialize ZRTP for media stream!");
        return free_resources(ret);
    }

    if ((ret = init_srtp_with_zrtp(rce_flags_, SRTP, srtp_, zrtp_)) != RTP_OK)
        return free_resources(ret);

    if ((ret = init_srtp_with_zrtp(rce_flags_, SRTCP, srtcp_, zrtp_)) != RTP_OK)
        return free_resources(ret);

    zrtp_->set_zrtp_busy(false);
    zrtp_->dh_has_finished(); // only after the DH stream has gotten its keys, do we let non-DH stream perform ZRTP
    install_packet_handlers();

    return RTP_OK;
}

rtp_error_t uvgrtp::media_stream::add_srtp_ctx(uint8_t *key, uint8_t *salt)
{
    if (!key || !salt)
        return RTP_INVALID_VALUE;

    unsigned int srtp_rce_flags = RCE_SRTP | RCE_SRTP_KMNGMNT_USER;
    rtp_error_t ret     = RTP_OK;

    if ((rce_flags_ & srtp_rce_flags) != srtp_rce_flags)
        return free_resources(RTP_NOT_SUPPORTED);

    // why are they local and remote key/salt the same?
    if ((ret = srtp_->init(SRTP, rce_flags_, key, key, salt, salt)) != RTP_OK) {
        UVG_LOG_WARN("Failed to initialize SRTP for media stream!");
        return free_resources(ret);
    }


    if ((ret = srtcp_->init(SRTCP, rce_flags_, key, key, salt, salt)) != RTP_OK) {
        UVG_LOG_WARN("Failed to initialize SRTCP for media stream!");
        return free_resources(ret);
    }

    return start_components();
}

rtp_error_t uvgrtp::media_stream::start_components()
{
    if (create_media(fmt_) != RTP_OK)
        return free_resources(RTP_MEMORY_ERROR);

    if (rce_flags_ & RCE_HOLEPUNCH_KEEPALIVE) {
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
            if (!(rce_flags_ & RCE_RTCP_MUX)) {
                rtcp_->set_network_addresses(local_address_, remote_address_, src_port_ + 1, dst_port_ + 1, ipv6_);
            }
            else {
                rtcp_->set_network_addresses(local_address_, remote_address_, src_port_, dst_port_, ipv6_);
                rtcp_->set_socket(socket_);
            }
            rtcp_->add_initial_participant(rtp_->get_clock_rate());
            bandwidth_ = get_default_bandwidth_kbps(fmt_);
            rtcp_->set_session_bandwidth(bandwidth_);

            uint16_t rtcp_port = src_port_ + 1;
            std::shared_ptr<uvgrtp::socket> rtcp_socket;
            std::shared_ptr<uvgrtp::rtcp_reader> rtcp_reader;

            if (!(rce_flags_ & RCE_RTCP_MUX)) {
                
                /* If RTCP is not multiplexed with RTP, configure the socket for RTCP:
                1. Fetch the socket for RTCP
                2. Get the RTCP reader from socketfactory. This was created automatically with the socket (type = 1)
                3. In RTCP reader, map our RTCP object to the REMOTE ssrc of this stream. If we are not doing
                   any socket multiplexing, it will be 0 by default */

                rtcp_socket = sfp_->get_socket_ptr(1, rtcp_port);
                rtcp_->set_socket(rtcp_socket);
                rtcp_reader = sfp_->get_rtcp_reader(rtcp_port);
                rtcp_reader->map_ssrc_to_rtcp(remote_ssrc_, rtcp_);
                rtcp_reader->set_socket(rtcp_socket);
            }
            rtcp_->start();
        }
    }

    if (rce_flags_ & RCE_SRTP_AUTHENTICATE_RTP) {
        if (ipv6_) {
            rtp_->set_payload_size(MAX_IPV6_MEDIA_PAYLOAD - UVG_AUTH_TAG_LENGTH);
        }
        else {
            rtp_->set_payload_size(MAX_IPV4_MEDIA_PAYLOAD - UVG_AUTH_TAG_LENGTH);
        }
    }

    initialized_ = true;
    return reception_flow_->start(socket_, rce_flags_);
}

rtp_error_t uvgrtp::media_stream::push_frame(uint8_t *data, size_t data_len, int rtp_flags)
{
    rtp_error_t ret = check_push_preconditions(rtp_flags, false);
    if (ret == RTP_OK)
    {
        if (rce_flags_ & RCE_HOLEPUNCH_KEEPALIVE)
            holepuncher_->notify();

        if (rtp_flags & RTP_COPY)
        {
            data = copy_frame(data, data_len);
            std::unique_ptr<uint8_t[]> data_copy(data);
            ret = media_->push_frame(remote_sockaddr_, remote_sockaddr_ip6_, std::move(data_copy), data_len, rtp_flags);
        }
        else
        {
            ret = media_->push_frame(remote_sockaddr_, remote_sockaddr_ip6_, data, data_len, rtp_flags);
        }
    }

    return ret;
}

rtp_error_t uvgrtp::media_stream::push_frame(std::unique_ptr<uint8_t[]> data, size_t data_len, int rtp_flags)
{
    rtp_error_t ret = check_push_preconditions(rtp_flags, true);
    if (ret == RTP_OK)
    {
        if (rce_flags_ & RCE_HOLEPUNCH_KEEPALIVE)
            holepuncher_->notify();

        // making a copy of a smart pointer does not make sense
        ret = media_->push_frame(remote_sockaddr_, remote_sockaddr_ip6_, std::move(data), data_len, rtp_flags);
    }

    return ret;
}

rtp_error_t uvgrtp::media_stream::push_frame(uint8_t *data, size_t data_len, uint32_t ts, int rtp_flags)
{
    rtp_error_t ret = check_push_preconditions(rtp_flags, false);
    if (ret == RTP_OK)
    {
        if (rce_flags_ & RCE_HOLEPUNCH_KEEPALIVE)
            holepuncher_->notify();

        rtp_->set_timestamp(ts);
        if (rtp_flags & RTP_COPY)
        {
            data = copy_frame(data, data_len);
            std::unique_ptr<uint8_t[]> data_copy(data);
            ret = media_->push_frame(remote_sockaddr_, remote_sockaddr_ip6_, std::move(data_copy), data_len, rtp_flags);
        }
        else
        {
            ret = media_->push_frame(remote_sockaddr_, remote_sockaddr_ip6_, data, data_len, rtp_flags);
        }
        rtp_->set_timestamp(INVALID_TS);
    }

    return ret;
}

rtp_error_t uvgrtp::media_stream::push_frame(uint8_t* data, size_t data_len, uint32_t ts, uint64_t ntp_ts, int rtp_flags)
{
    rtp_error_t ret = check_push_preconditions(rtp_flags, false);
    if (ret == RTP_OK)
    {
        if (rce_flags_ & RCE_HOLEPUNCH_KEEPALIVE)
            holepuncher_->notify();

        rtp_->set_timestamp(ts);
        rtp_->set_sampling_ntp(ntp_ts);
        if (rtp_flags & RTP_COPY)
        {
            data = copy_frame(data, data_len);
            std::unique_ptr<uint8_t[]> data_copy(data);
            ret = media_->push_frame(remote_sockaddr_, remote_sockaddr_ip6_, std::move(data_copy), data_len, rtp_flags);
        }
        else
        {
            ret = media_->push_frame(remote_sockaddr_, remote_sockaddr_ip6_, data, data_len, rtp_flags);
        }
        rtp_->set_timestamp(INVALID_TS);
    }

    return ret;
}

rtp_error_t uvgrtp::media_stream::push_frame(std::unique_ptr<uint8_t[]> data, size_t data_len, uint32_t ts, int rtp_flags)
{
    rtp_error_t ret = check_push_preconditions(rtp_flags, true);
    if (ret == RTP_OK)
    {
        if (rce_flags_ & RCE_HOLEPUNCH_KEEPALIVE)
            holepuncher_->notify();

        // making a copy of a smart pointer does not make sense
        rtp_->set_timestamp(ts);
        ret = media_->push_frame(remote_sockaddr_, remote_sockaddr_ip6_, std::move(data), data_len, rtp_flags);
        rtp_->set_timestamp(INVALID_TS);
    }

    return ret;
}

rtp_error_t uvgrtp::media_stream::push_frame(std::unique_ptr<uint8_t[]> data, size_t data_len, uint32_t ts, uint64_t ntp_ts, int rtp_flags)
{
    rtp_error_t ret = check_push_preconditions(rtp_flags, true);
    if (ret == RTP_OK)
    {
        if (rce_flags_ & RCE_HOLEPUNCH_KEEPALIVE)
            holepuncher_->notify();

        // making a copy of a smart pointer does not make sense
        rtp_->set_timestamp(ts);
        rtp_->set_sampling_ntp(ntp_ts);
        ret = media_->push_frame(remote_sockaddr_, remote_sockaddr_ip6_, std::move(data), data_len, rtp_flags);
        rtp_->set_timestamp(INVALID_TS);
    }

    return ret;
}
/* Disabled for now
rtp_error_t uvgrtp::media_stream::push_user_packet(uint8_t* data, uint32_t len)
{
    if (rce_flags_ & RCE_RECEIVE_ONLY) {
        UVG_LOG_WARN("Cannot send user packets from a RECEIVE_ONLY stream");
        return RTP_SEND_ERROR;
    }
    sockaddr_in6 addr6;
    sockaddr_in addr;
    if (ipv6_) {
        addr6 = uvgrtp::socket::create_ip6_sockaddr(remote_address_, dst_port_);
    }
    else {
        addr = uvgrtp::socket::create_sockaddr(AF_INET, remote_address_, dst_port_);
    }
    UVG_LOG_DEBUG("Sending user packet");
    return socket_->sendto(addr, addr6, data, len, 0);
}

rtp_error_t uvgrtp::media_stream::install_user_receive_hook(void* arg, void (*hook)(void*, uint8_t* payload, uint32_t len))
{
    if (!initialized_) {
        UVG_LOG_ERROR("RTP context has not been initialized fully, cannot continue!");
        return RTP_NOT_INITIALIZED;
    }

    if (!hook)
        return RTP_INVALID_VALUE;

    return reception_flow_->install_user_hook(arg, hook);;

}*/

uvgrtp::frame::rtp_frame *uvgrtp::media_stream::pull_frame()
{
    if (!check_pull_preconditions()) {
        return nullptr;
    }
    // If the remote_ssrc is set, only pull frames that come from this ssrc
    if (remote_ssrc_.get()->load() != ssrc_.get()->load() + 1) {
        return reception_flow_->pull_frame(remote_ssrc_);
    }
    return reception_flow_->pull_frame();

}

uvgrtp::frame::rtp_frame *uvgrtp::media_stream::pull_frame(size_t timeout_ms)
{
    if (!check_pull_preconditions()) {
        return nullptr;
    }
    // If the remote_ssrc is set, only pull frames that come from this ssrc
    if (remote_ssrc_.get()->load() != ssrc_.get()->load() + 1) {
        return reception_flow_->pull_frame(timeout_ms, remote_ssrc_);
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

rtp_error_t uvgrtp::media_stream::check_push_preconditions(int rtp_flags, bool smart_pointer)
{
    if (!initialized_) {
        UVG_LOG_ERROR("RTP context has not been initialized fully, cannot continue!");
        return RTP_NOT_INITIALIZED;
    }

    if (remote_address_ == "" && dst_port_ != 0)
    {
        UVG_LOG_ERROR("Cannot push frame if remote address and port have not been provided!");
        return RTP_INVALID_VALUE;
    }

    if (rtp_flags & RTP_OBSOLETE)
    {
        UVG_LOG_WARN("Detected an obsolete RTP flag, consider updating your flags");
    }

    if (rce_flags_ & RCE_RECEIVE_ONLY)
    {
        UVG_LOG_WARN("Stream is designated as RECEIVE ONLY");
        return RTP_GENERIC_ERROR;
    }

    if (smart_pointer && (rtp_flags & RTP_COPY))
    {
        UVG_LOG_ERROR("Copying a smart pointer does not make sense since the original would be lost");
    }

    return RTP_OK;
}

uint8_t* uvgrtp::media_stream::copy_frame(uint8_t* original, size_t data_len)
{
    uint8_t* copy = new uint8_t[data_len];
    memcpy(copy, original, data_len);
    return copy;
}

rtp_error_t uvgrtp::media_stream::install_receive_hook(void *arg, void (*hook)(void *, uvgrtp::frame::rtp_frame *))
{
    if (!initialized_) {
        UVG_LOG_ERROR("RTP context has not been initialized fully, cannot continue!");
        return RTP_NOT_INITIALIZED;
    }

    if (!hook) {
        return RTP_INVALID_VALUE;
    }
    return reception_flow_->install_receive_hook(arg, hook, remote_ssrc_.get()->load());
}

rtp_error_t uvgrtp::media_stream::configure_ctx(int rcc_flag, ssize_t value)
{
    rtp_error_t ret = RTP_OK;

    if (rcc_flag == RCC_SSRC) {
        if (value <= 0 || value > (ssize_t)UINT32_MAX)
            return RTP_INVALID_VALUE;

        *ssrc_ = (uint32_t)value;
        return ret;
    }
    else if (rcc_flag == RCC_REMOTE_SSRC) {
        if (value <= 0 || value > (ssize_t)UINT32_MAX)
            return RTP_INVALID_VALUE;
        if (reception_flow_) {
            reception_flow_->update_remote_ssrc(remote_ssrc_.get()->load(), (uint32_t)value);
        }
        *remote_ssrc_ = (uint32_t)value;
        
        return ret;
    }
    else if (rcc_flag == RCC_POLL_TIMEOUT) {
        if (reception_flow_) {
            reception_flow_->set_poll_timeout_ms((int)value);
        }
        return ret;
    }

    if (!initialized_) {
        UVG_LOG_ERROR("RTP context has not been initialized fully, cannot continue!");
        return RTP_NOT_INITIALIZED;
    }

    switch (rcc_flag) {
        case RCC_UDP_SND_BUF_SIZE: {
            if (value <= 0)
                return RTP_INVALID_VALUE;

            int buf_size = (int)value;
            snd_buf_size_ = buf_size;
            ret = socket_->setsockopt(SOL_SOCKET, SO_SNDBUF, (const char*)&buf_size, sizeof(int));
            break;
        }
        case RCC_UDP_RCV_BUF_SIZE: {
            if (value <= 0)
                return RTP_INVALID_VALUE;

            int buf_size = (int)value;
            rcv_buf_size_ = buf_size;
            ret = socket_->setsockopt(SOL_SOCKET, SO_RCVBUF, (const char*)&buf_size, sizeof(int));
            break;
        }
        case RCC_RING_BUFFER_SIZE: {
            if (value <= 0)
                return RTP_INVALID_VALUE;

            reception_flow_->set_buffer_size(value);
            break;
        }
        case RCC_PKT_MAX_DELAY: {
            if (value <= 0)
                return RTP_INVALID_VALUE;

            rtp_->set_pkt_max_delay(value);
            break;
        }
        case RCC_DYN_PAYLOAD_TYPE: {
            if (value <= 0 || (ssize_t)UINT8_MAX < value)
                return RTP_INVALID_VALUE;

            rtp_->set_dynamic_payload((uint8_t)value);
            break;
        }
        case RCC_CLOCK_RATE: {
            if (value <= 0 || (ssize_t)UINT32_MAX < value)
                return RTP_INVALID_VALUE;

            rtp_->set_clock_rate((uint32_t)value);
            break;
        }
        case RCC_MTU_SIZE: {
            ssize_t hdr      = IPV4_HDR_SIZE + UDP_HDR_SIZE + RTP_HDR_SIZE;
            if (rce_flags_ & RCE_SRTP_AUTHENTICATE_RTP)
                hdr += UVG_AUTH_TAG_LENGTH;

            if (value <= hdr)
                return RTP_INVALID_VALUE;

            if (value > (ssize_t)UINT16_MAX) {
                UVG_LOG_ERROR("Payload size (%zd) is larger than maximum UDP datagram size (%u)",
                        value, UINT16_MAX);
                return RTP_INVALID_VALUE;
            }

            rtp_->set_payload_size(value - hdr);

            // auth tag is always included with SRTP and RTCP has a header for each packet within a compound frame
            rtcp_->set_payload_size(          value - (IPV4_HDR_SIZE + UDP_HDR_SIZE)); 

            reception_flow_->set_payload_size(value - (IPV4_HDR_SIZE + UDP_HDR_SIZE)); // largest packet we can get from socket
            break;
        }
        case RCC_FPS_NUMERATOR: {
            fps_numerator_ = value;

            if (value > 0 && (rce_flags_ & RCE_SYSTEM_CALL_CLUSTERING)) {
                UVG_LOG_WARN("Setting FPS numerator will disable System Call Clustering (SCC)");
            }

            media_->set_fps(fps_numerator_, fps_denominator_);
            break;
        }
        case RCC_FPS_DENOMINATOR: {
            fps_denominator_ = value;

            if (value > 0 && (rce_flags_ & RCE_SYSTEM_CALL_CLUSTERING)) {
                UVG_LOG_WARN("Setting FPS denominator will disable System Call Clustering (SCC)");
            }

            media_->set_fps(fps_numerator_, fps_denominator_);
            break;
        }
        case RCC_SESSION_BANDWIDTH: {
            bandwidth_ = (uint32_t)value;
            // TODO: Is there a max value for bandwidth?
            if (value <= 0) {
                UVG_LOG_WARN("Bandwidth cannot be negative");
                return RTP_INVALID_VALUE;
            }
            if (rtcp_) {
                rtcp_->set_session_bandwidth(bandwidth_);
            }
            break;
        }
        case RCC_SSRC: {
            if (value <= 0 || value > (ssize_t)UINT32_MAX)
                return RTP_INVALID_VALUE;

            *ssrc_ = (uint32_t)value;
            break;
        }
        case RCC_REMOTE_SSRC: {
            if (value <= 0 || value > (ssize_t)UINT32_MAX)
                return RTP_INVALID_VALUE;

            *remote_ssrc_ = (uint32_t)value;
            break;
        }
        default:
            return RTP_INVALID_VALUE;
    }

    return ret;
}

int uvgrtp::media_stream::get_configuration_value(int rcc_flag)
{
    int ret = -1;

    if (rcc_flag == RCC_SSRC) {
        return ssrc_.get()->load();
    }
    else if (rcc_flag == RCC_REMOTE_SSRC) {
        return remote_ssrc_.get()->load();
    }

    if (!initialized_) {
        UVG_LOG_ERROR("RTP context has not been initialized fully, cannot continue!");
        return RTP_NOT_INITIALIZED;
    }

    switch (rcc_flag) {
        case RCC_UDP_SND_BUF_SIZE: {
            return snd_buf_size_;
        }
        case RCC_UDP_RCV_BUF_SIZE: {
            return rcv_buf_size_;
        }
        case RCC_RING_BUFFER_SIZE: {
            return (int)reception_flow_->get_buffer_size();
        }
        case RCC_PKT_MAX_DELAY: {
            return (int)rtp_->get_pkt_max_delay();
        }
        case RCC_DYN_PAYLOAD_TYPE: {
            return (int)rtp_->get_dynamic_payload();
        }
        case RCC_CLOCK_RATE: {
            return (int)rtp_->get_clock_rate();
        }
        case RCC_MTU_SIZE: {
            return (int)rtp_->get_payload_size();
        }
        case RCC_FPS_NUMERATOR: {
            return (int)fps_numerator_;
        }
        case RCC_FPS_DENOMINATOR: {
            return (int)fps_denominator_;
        }
        case RCC_SESSION_BANDWIDTH: {
            return (int)bandwidth_;
        }
        case RCC_POLL_TIMEOUT: {
            return reception_flow_->get_poll_timeout_ms();
        }
        default:
            ret = -1;
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

    return *ssrc_.get();
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
