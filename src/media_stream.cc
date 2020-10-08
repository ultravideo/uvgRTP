#include <cstring>
#include <errno.h>

#include "debug.hh"
#include "media_stream.hh"
#include "random.hh"

#include "formats/h264.hh"
#include "formats/h265.hh"

#define INVALID_TS UINT64_MAX

uvg_rtp::media_stream::media_stream(std::string addr, int src_port, int dst_port, rtp_format_t fmt, int flags):
    srtp_(nullptr),
    srtcp_(nullptr),
    socket_(nullptr),
    rtp_(nullptr),
    rtcp_(nullptr),
    ctx_config_(),
    media_config_(nullptr),
    initialized_(false),
    rtp_handler_key_(0),
    pkt_dispatcher_(nullptr),
    dispatcher_thread_(nullptr),
    media_(nullptr)
{
    fmt_      = fmt;
    addr_     = addr;
    laddr_    = "";
    flags_    = flags;
    src_port_ = src_port;
    dst_port_ = dst_port;
    key_      = uvg_rtp::random::generate_32();

    ctx_config_.flags = flags;
}

uvg_rtp::media_stream::media_stream(
    std::string remote_addr, std::string local_addr,
    int src_port, int dst_port,
    rtp_format_t fmt, int flags
):
    media_stream(remote_addr, src_port, dst_port, fmt, flags)
{
    laddr_ = local_addr;
}

uvg_rtp::media_stream::~media_stream()
{
    pkt_dispatcher_->stop();

    if (ctx_config_.flags & RCE_RTCP)
        rtcp_->stop();

    delete socket_;
    delete rtcp_;
    delete rtp_;
    delete srtp_;
    delete srtcp_;
    delete pkt_dispatcher_;
    delete dispatcher_thread_;
    delete media_;
}

rtp_error_t uvg_rtp::media_stream::init_connection()
{
    rtp_error_t ret = RTP_OK;

    if (!(socket_ = new uvg_rtp::socket(ctx_config_.flags)))
        return ret;

    if ((ret = socket_->init(AF_INET, SOCK_DGRAM, 0)) != RTP_OK)
        return ret;

#ifdef _WIN32
    /* Make the socket non-blocking */
    int enabled = 1;

    if (::ioctlsocket(socket_->get_raw_socket(), FIONBIO, (u_long *)&enabled) < 0)
        LOG_ERROR("Failed to make the socket non-blocking!");
#endif

    if (laddr_ != "") {
        sockaddr_in bind_addr = socket_->create_sockaddr(AF_INET, laddr_, src_port_);
        socket_t socket       = socket_->get_raw_socket();

        if (bind(socket, (struct sockaddr *)&bind_addr, sizeof(bind_addr)) == -1) {
            log_platform_error("bind(2) failed");
            return RTP_BIND_ERROR;
        }
    } else {
        if ((ret = socket_->bind(AF_INET, INADDR_ANY, src_port_)) != RTP_OK)
            return ret;
    }

    /* Set the default UDP send/recv buffer sizes to 4MB as on Windows
     * the default size is way too small for a larger video conference */
    int buf_size = 4 * 1024 * 1024;

    if ((ret = socket_->setsockopt(SOL_SOCKET, SO_SNDBUF, (const char *)&buf_size, sizeof(int))) != RTP_OK)
        return ret;

    if ((ret = socket_->setsockopt(SOL_SOCKET, SO_RCVBUF, (const char *)&buf_size, sizeof(int))) != RTP_OK)
        return ret;

    addr_out_ = socket_->create_sockaddr(AF_INET, addr_, dst_port_);
    socket_->set_sockaddr(addr_out_);

    return ret;
}

rtp_error_t uvg_rtp::media_stream::init()
{
    if (init_connection() != RTP_OK) {
        log_platform_error("Failed to initialize the underlying socket");
        return RTP_GENERIC_ERROR;
    }

    if (!(pkt_dispatcher_ = new uvg_rtp::pkt_dispatcher()))
        return RTP_MEMORY_ERROR;

    if (!(rtp_ = new uvg_rtp::rtp(fmt_))) {
        delete pkt_dispatcher_;
        return RTP_MEMORY_ERROR;
    }

    if (!(rtcp_ = new uvg_rtp::rtcp(rtp_, ctx_config_.flags))) {
        delete rtp_;
        delete pkt_dispatcher_;
        return RTP_MEMORY_ERROR;
    }

    socket_->install_handler(rtcp_, rtcp_->send_packet_handler_vec);

    rtp_handler_key_ = pkt_dispatcher_->install_handler(rtp_->packet_handler);
    pkt_dispatcher_->install_aux_handler(rtp_handler_key_, rtcp_, rtcp_->recv_packet_handler, nullptr);

    switch (fmt_) {
        case RTP_FORMAT_H265:
            media_ = new uvg_rtp::formats::h265(socket_, rtp_, ctx_config_.flags);
            pkt_dispatcher_->install_aux_handler(
                rtp_handler_key_,
                dynamic_cast<uvg_rtp::formats::h265 *>(media_)->get_h265_frame_info(),
                dynamic_cast<uvg_rtp::formats::h265 *>(media_)->packet_handler,
                dynamic_cast<uvg_rtp::formats::h265 *>(media_)->frame_getter
            );
            break;

        case RTP_FORMAT_H264:
            media_ = new uvg_rtp::formats::h264(socket_, rtp_, ctx_config_.flags);
            pkt_dispatcher_->install_aux_handler(
                rtp_handler_key_,
                dynamic_cast<uvg_rtp::formats::h265 *>(media_)->get_h265_frame_info(),
                dynamic_cast<uvg_rtp::formats::h264 *>(media_)->packet_handler,
                nullptr
            );
            break;

        case RTP_FORMAT_OPUS:
        case RTP_FORMAT_GENERIC:
            media_ = new uvg_rtp::formats::media(socket_, rtp_, ctx_config_.flags);
            pkt_dispatcher_->install_aux_handler(rtp_handler_key_, nullptr, media_->packet_handler, nullptr);
            break;

        default:
            LOG_ERROR("Unknown payload format %u\n", fmt_);
            media_ = nullptr;
    }

    if (!media_) {
        delete rtp_;
        delete rtcp_;
        delete pkt_dispatcher_;
        return RTP_MEMORY_ERROR;
    }

    if (ctx_config_.flags & RCE_RTCP) {
        rtcp_->add_participant(addr_, src_port_ + 1, dst_port_ + 1, rtp_->get_clock_rate());
        rtcp_->start();
    }

    initialized_ = true;
    return pkt_dispatcher_->start(socket_, ctx_config_.flags);
}

rtp_error_t uvg_rtp::media_stream::init(uvg_rtp::zrtp *zrtp)
{
    rtp_error_t ret;

    if (init_connection() != RTP_OK) {
        log_platform_error("Failed to initialize the underlying socket");
        return RTP_GENERIC_ERROR;
    }

    if (!(pkt_dispatcher_ = new uvg_rtp::pkt_dispatcher()))
        return RTP_MEMORY_ERROR;

    if (!(rtp_ = new uvg_rtp::rtp(fmt_))) {
        delete pkt_dispatcher_;
        return RTP_MEMORY_ERROR;
    }

    if ((ret = zrtp->init(rtp_->get_ssrc(), socket_, addr_out_)) != RTP_OK) {
        LOG_WARN("Failed to initialize ZRTP for media stream!");
        delete rtp_;
        delete pkt_dispatcher_;
        return ret;
    }

    if (!(srtp_ = new uvg_rtp::srtp())) {
        delete rtp_;
        delete pkt_dispatcher_;
        return RTP_MEMORY_ERROR;
    }

    if ((ret = srtp_->init_zrtp(SRTP, ctx_config_.flags, rtp_, zrtp)) != RTP_OK) {
        LOG_WARN("Failed to initialize SRTP for media stream!");
        delete rtp_;
        delete srtp_;
        delete pkt_dispatcher_;
        return ret;
    }

    if (!(srtcp_ = new uvg_rtp::srtcp())) {
        delete rtp_;
        delete srtp_;
        delete pkt_dispatcher_;
        return RTP_MEMORY_ERROR;
    }

    if ((ret = srtcp_->init_zrtp(SRTCP, ctx_config_.flags, rtp_, zrtp)) != RTP_OK) {
        LOG_ERROR("Failed to initialize SRTCP for media stream!");
        delete rtp_;
        delete srtp_;
        delete srtcp_;
        delete pkt_dispatcher_;
        return ret;
    }

    if (!(rtcp_ = new uvg_rtp::rtcp(rtp_, srtcp_, ctx_config_.flags))) {
        delete rtp_;
        delete srtp_;
        delete srtcp_;
        delete pkt_dispatcher_;
        return RTP_MEMORY_ERROR;
    }

    socket_->install_handler(rtcp_, rtcp_->send_packet_handler_vec);
    socket_->install_handler(srtp_, srtp_->send_packet_handler);

    rtp_handler_key_  = pkt_dispatcher_->install_handler(rtp_->packet_handler);
    zrtp_handler_key_ = pkt_dispatcher_->install_handler(zrtp->packet_handler);

    pkt_dispatcher_->install_aux_handler(rtp_handler_key_, rtcp_, rtcp_->recv_packet_handler, nullptr);
    pkt_dispatcher_->install_aux_handler(rtp_handler_key_, srtp_, srtp_->recv_packet_handler, nullptr);

    switch (fmt_) {
        case RTP_FORMAT_H265:
            media_ = new uvg_rtp::formats::h265(socket_, rtp_, ctx_config_.flags);
            pkt_dispatcher_->install_aux_handler(
                rtp_handler_key_,
                nullptr,
                dynamic_cast<uvg_rtp::formats::h265 *>(media_)->packet_handler,
                dynamic_cast<uvg_rtp::formats::h265 *>(media_)->frame_getter
            );
            break;

        case RTP_FORMAT_H264:
            media_ = new uvg_rtp::formats::h264(socket_, rtp_, ctx_config_.flags);
            pkt_dispatcher_->install_aux_handler(
                rtp_handler_key_,
                dynamic_cast<uvg_rtp::formats::h265 *>(media_)->get_h265_frame_info(),
                dynamic_cast<uvg_rtp::formats::h264 *>(media_)->packet_handler,
                nullptr
            );
            break;

        case RTP_FORMAT_OPUS:
        case RTP_FORMAT_GENERIC:
            media_ = new uvg_rtp::formats::media(socket_, rtp_, ctx_config_.flags);
            pkt_dispatcher_->install_aux_handler(rtp_handler_key_, nullptr, media_->packet_handler, nullptr);
            break;

        default:
            LOG_ERROR("Unknown payload format %u\n", fmt_);
    }

    if (!media_) {
        delete rtp_;
        delete srtp_;
        delete srtcp_;
        delete rtcp_;
        delete pkt_dispatcher_;
        return RTP_MEMORY_ERROR;
    }

    if (ctx_config_.flags & RCE_RTCP) {
        rtcp_->add_participant(addr_, src_port_ + 1, dst_port_ + 1, rtp_->get_clock_rate());
        rtcp_->start();
    }

    if (ctx_config_.flags & RCE_SRTP_AUTHENTICATE_RTP)
        rtp_->set_payload_size(MAX_PAYLOAD - AUTH_TAG_LENGTH);

    initialized_ = true;
    return pkt_dispatcher_->start(socket_, ctx_config_.flags);
}

rtp_error_t uvg_rtp::media_stream::add_srtp_ctx(uint8_t *key, uint8_t *salt)
{
    if (!key || !salt)
        return RTP_INVALID_VALUE;

    unsigned srtp_flags = RCE_SRTP | RCE_SRTP_KMNGMNT_USER;
    rtp_error_t ret     = RTP_OK;

    if (init_connection() != RTP_OK) {
        log_platform_error("Failed to initialize the underlying socket");
        return RTP_GENERIC_ERROR;
    }

    if ((flags_ & srtp_flags) != srtp_flags)
        return RTP_NOT_SUPPORTED;

    if (!(pkt_dispatcher_ = new uvg_rtp::pkt_dispatcher()))
        return RTP_MEMORY_ERROR;

    if (!(rtp_ = new uvg_rtp::rtp(fmt_))) {
        delete pkt_dispatcher_;
        return RTP_MEMORY_ERROR;
    }

    if (!(srtp_ = new uvg_rtp::srtp())) {
        delete rtp_;
        delete pkt_dispatcher_;
        return RTP_MEMORY_ERROR;
    }

    if ((ret = srtp_->init_user(SRTP, ctx_config_.flags, key, salt)) != RTP_OK) {
        LOG_WARN("Failed to initialize SRTP for media stream!");
        return ret;
    }

    if (!(srtcp_ = new uvg_rtp::srtcp())) {
        delete rtp_;
        delete srtp_;
        delete pkt_dispatcher_;
        return RTP_MEMORY_ERROR;
    }

    if ((ret = srtcp_->init_user(SRTCP, ctx_config_.flags, key, salt)) != RTP_OK) {
        LOG_WARN("Failed to initialize SRTCP for media stream!");
        return ret;
    }

    if (!(rtcp_ = new uvg_rtp::rtcp(rtp_, srtcp_, ctx_config_.flags))) {
        delete rtp_;
        delete srtp_;
        delete srtcp_;
        delete pkt_dispatcher_;
        return RTP_MEMORY_ERROR;
    }

    socket_->install_handler(rtcp_, rtcp_->send_packet_handler_vec);
    socket_->install_handler(srtp_, srtp_->send_packet_handler);

    rtp_handler_key_ = pkt_dispatcher_->install_handler(rtp_->packet_handler);

    pkt_dispatcher_->install_aux_handler(rtp_handler_key_, rtcp_, rtcp_->recv_packet_handler, nullptr);
    pkt_dispatcher_->install_aux_handler(rtp_handler_key_, srtp_, srtp_->recv_packet_handler, nullptr);

    switch (fmt_) {
        case RTP_FORMAT_H265:
            media_ = new uvg_rtp::formats::h265(socket_, rtp_, ctx_config_.flags);
            pkt_dispatcher_->install_aux_handler(
                rtp_handler_key_,
                nullptr,
                dynamic_cast<uvg_rtp::formats::h265 *>(media_)->packet_handler,
                dynamic_cast<uvg_rtp::formats::h265 *>(media_)->frame_getter
            );
            break;

        case RTP_FORMAT_H264:
            media_ = new uvg_rtp::formats::h264(socket_, rtp_, ctx_config_.flags);
            pkt_dispatcher_->install_aux_handler(
                rtp_handler_key_,
                dynamic_cast<uvg_rtp::formats::h265 *>(media_)->get_h265_frame_info(),
                dynamic_cast<uvg_rtp::formats::h264 *>(media_)->packet_handler,
                nullptr
            );
            break;

        case RTP_FORMAT_OPUS:
        case RTP_FORMAT_GENERIC:
            media_ = new uvg_rtp::formats::media(socket_, rtp_, ctx_config_.flags);
            pkt_dispatcher_->install_aux_handler(rtp_handler_key_, nullptr, media_->packet_handler, nullptr);
            break;

        default:
            LOG_ERROR("Unknown payload format %u\n", fmt_);
    }

    if (!media_) {
        delete rtp_;
        delete srtp_;
        delete srtcp_;
        delete rtcp_;
        delete pkt_dispatcher_;
        return RTP_MEMORY_ERROR;
    }

    if (ctx_config_.flags & RCE_RTCP) {
        rtcp_->add_participant(addr_, src_port_ + 1, dst_port_ + 1, rtp_->get_clock_rate());
        rtcp_->start();
    }

    if (ctx_config_.flags & RCE_SRTP_AUTHENTICATE_RTP)
        rtp_->set_payload_size(MAX_PAYLOAD - AUTH_TAG_LENGTH);

    initialized_ = true;
    return pkt_dispatcher_->start(socket_, ctx_config_.flags);

}

rtp_error_t uvg_rtp::media_stream::push_frame(uint8_t *data, size_t data_len, int flags)
{
    if (!initialized_) {
        LOG_ERROR("RTP context has not been initialized fully, cannot continue!");
        return RTP_NOT_INITIALIZED;
    }

    return media_->push_frame(data, data_len, flags);
}

rtp_error_t uvg_rtp::media_stream::push_frame(std::unique_ptr<uint8_t[]> data, size_t data_len, int flags)
{
    if (!initialized_) {
        LOG_ERROR("RTP context has not been initialized fully, cannot continue!");
        return RTP_NOT_INITIALIZED;
    }

    return media_->push_frame(std::move(data), data_len, flags);
}

rtp_error_t uvg_rtp::media_stream::push_frame(uint8_t *data, size_t data_len, uint32_t ts, int flags)
{
    rtp_error_t ret = RTP_GENERIC_ERROR;

    if (!initialized_) {
        LOG_ERROR("RTP context has not been initialized fully, cannot continue!");
        return RTP_NOT_INITIALIZED;
    }

    rtp_->set_timestamp(ts);
    ret = media_->push_frame(data, data_len, flags);
    rtp_->set_timestamp(INVALID_TS);

    return ret;
}

rtp_error_t uvg_rtp::media_stream::push_frame(std::unique_ptr<uint8_t[]> data, size_t data_len, uint32_t ts, int flags)
{
    rtp_error_t ret = RTP_GENERIC_ERROR;

    if (!initialized_) {
        LOG_ERROR("RTP context has not been initialized fully, cannot continue!");
        return RTP_NOT_INITIALIZED;
    }

    rtp_->set_timestamp(ts);
    ret = media_->push_frame(std::move(data), data_len, flags);
    rtp_->set_timestamp(INVALID_TS);

    return ret;
}

uvg_rtp::frame::rtp_frame *uvg_rtp::media_stream::pull_frame()
{
    if (!initialized_) {
        LOG_ERROR("RTP context has not been initialized fully, cannot continue!");
        rtp_errno = RTP_NOT_INITIALIZED;
        return nullptr;
    }

    return pkt_dispatcher_->pull_frame();
}

uvg_rtp::frame::rtp_frame *uvg_rtp::media_stream::pull_frame(size_t timeout)
{
    if (!initialized_) {
        LOG_ERROR("RTP context has not been initialized fully, cannot continue!");
        rtp_errno = RTP_NOT_INITIALIZED;
        return nullptr;
    }

    return pkt_dispatcher_->pull_frame(timeout);
}

rtp_error_t uvg_rtp::media_stream::install_receive_hook(void *arg, void (*hook)(void *, uvg_rtp::frame::rtp_frame *))
{
    if (!initialized_) {
        LOG_ERROR("RTP context has not been initialized fully, cannot continue!");
        return RTP_NOT_INITIALIZED;
    }

    if (!hook)
        return RTP_INVALID_VALUE;

    pkt_dispatcher_->install_receive_hook(arg, hook);

    return RTP_OK;
}

rtp_error_t uvg_rtp::media_stream::install_deallocation_hook(void (*hook)(void *))
{
    if (!initialized_) {
        LOG_ERROR("RTP context has not been initialized fully, cannot continue!");
        return RTP_NOT_INITIALIZED;
    }

    if (!hook)
        return RTP_INVALID_VALUE;

    /* TODO:  */

    return RTP_OK;
}

rtp_error_t uvg_rtp::media_stream::install_notify_hook(void *arg, void (*hook)(void *, int))
{
    (void)arg, (void)hook;

    if (!initialized_) {
        LOG_ERROR("RTP context has not been initialized fully, cannot continue!");
        return RTP_NOT_INITIALIZED;
    }

    if (!hook)
        return RTP_INVALID_VALUE;

    /* TODO:  */

    return RTP_OK;
}

void uvg_rtp::media_stream::set_media_config(void *config)
{
    media_config_ = config;
}

void *uvg_rtp::media_stream::get_media_config()
{
    return media_config_;
}

rtp_error_t uvg_rtp::media_stream::configure_ctx(int flag, ssize_t value)
{
    if (!initialized_) {
        LOG_ERROR("RTP context has not been initialized fully, cannot continue!");
        return RTP_NOT_INITIALIZED;
    }

    rtp_error_t ret = RTP_OK;

    switch (flag) {
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

            int buf_size = (int)value;
            if ((ret = socket_->setsockopt(SOL_SOCKET, SO_RCVBUF, (const char *)&buf_size, sizeof(int))) != RTP_OK)
                return ret;
        }
        break;

        default:
            return RTP_INVALID_VALUE;
    }

    return ret;
}

uint32_t uvg_rtp::media_stream::get_key()
{
    return key_;
}

rtp_error_t uvg_rtp::media_stream::set_dynamic_payload(uint8_t payload)
{
    if (!initialized_) {
        LOG_ERROR("RTP context has not been initialized fully, cannot continue!");
        return RTP_NOT_INITIALIZED;
    }

    rtp_->set_dynamic_payload(payload);

    return RTP_OK;
}

uvg_rtp::rtcp *uvg_rtp::media_stream::get_rtcp()
{
    return rtcp_;
}
