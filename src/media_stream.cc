#include <cstring>
#include <errno.h>

#include "debug.hh"
#include "media_stream.hh"
#include "random.hh"

#define INVALID_TS UINT64_MAX

uvg_rtp::media_stream::media_stream(std::string addr, int src_port, int dst_port, rtp_format_t fmt, int flags):
    srtp_(nullptr),
    socket_(flags),
    sender_(nullptr),
    receiver_(nullptr),
    rtp_(nullptr),
    rtcp_(nullptr),
    ctx_config_(),
    media_config_(nullptr),
    initialized_(false)
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
    if (initialized_) {
        sender_->destroy();
        receiver_->stop();
    }

    delete sender_;
    delete receiver_;
    delete rtcp_;
    delete rtp_;
    delete srtp_;
}

rtp_error_t uvg_rtp::media_stream::init_connection()
{
    rtp_error_t ret = RTP_OK;

    if ((ret = socket_.init(AF_INET, SOCK_DGRAM, 0)) != RTP_OK)
        return ret;

#ifdef _WIN32
    /* Make the socket non-blocking */
    int enabled = 1;

    if (::ioctlsocket(socket_.get_raw_socket(), FIONBIO, (u_long *)&enabled) < 0)
        LOG_ERROR("Failed to make the socket non-blocking!");
#endif

    if (laddr_ != "") {
        sockaddr_in bind_addr = socket_.create_sockaddr(AF_INET, laddr_, src_port_);
        socket_t socket       = socket_.get_raw_socket();

        if (bind(socket, (struct sockaddr *)&bind_addr, sizeof(bind_addr)) == -1) {
#ifdef __linux__
            LOG_ERROR("Bind failed: %s!", strerror(errno));
#else
            LOG_ERROR("Bind failed!");
            win_get_last_error();
#endif
            return RTP_BIND_ERROR;
        }
    } else {
        if ((ret = socket_.bind(AF_INET, INADDR_ANY, src_port_)) != RTP_OK)
            return ret;
    }

    addr_out_ = socket_.create_sockaddr(AF_INET, addr_, dst_port_);
    socket_.set_sockaddr(addr_out_);

    return ret;
}

rtp_error_t uvg_rtp::media_stream::init()
{
    if (init_connection() != RTP_OK) {
        LOG_ERROR("Failed to initialize the underlying socket: %s!", strerror(errno));
        return RTP_GENERIC_ERROR;
    }

    rtp_      = new uvg_rtp::rtp(fmt_);
    sender_   = new uvg_rtp::sender(socket_, ctx_config_, fmt_, rtp_);
    receiver_ = new uvg_rtp::receiver(socket_, ctx_config_, fmt_, rtp_);

    sender_->init();
    receiver_->start();

    initialized_ = true;

    return RTP_OK;
}

#ifdef __RTP_CRYPTO__
rtp_error_t uvg_rtp::media_stream::init(uvg_rtp::zrtp *zrtp)
{
    if (init_connection() != RTP_OK) {
        LOG_ERROR("Failed to initialize the underlying socket: %s!", strerror(errno));
        return RTP_GENERIC_ERROR;
    }

    /* First initialize the RTP context for this media stream (SSRC, sequence number, etc.)
     * Then initialize ZRTP and using ZRTP, initialize SRTP.
     *
     * When ZRTP and SRTP have been initialized, create sender and receiver for the media type
     * before returning the media stream for user */
    rtp_error_t ret = RTP_OK;

    if ((rtp_ = new uvg_rtp::rtp(fmt_)) == nullptr)
        return RTP_MEMORY_ERROR;

    if ((ret = zrtp->init(rtp_->get_ssrc(), socket_.get_raw_socket(), addr_out_)) != RTP_OK) {
        LOG_WARN("Failed to initialize ZRTP for media stream!");
        return ret;
    }

    if ((srtp_ = new uvg_rtp::srtp()) == nullptr)
        return RTP_MEMORY_ERROR;

    if ((ret = srtp_->init_zrtp(SRTP, rtp_, zrtp)) != RTP_OK) {
        LOG_WARN("Failed to initialize SRTP for media stream!");
        return ret;
    }

    socket_.set_srtp(srtp_);

    sender_   = new uvg_rtp::sender(socket_, ctx_config_, fmt_, rtp_);
    receiver_ = new uvg_rtp::receiver(socket_, ctx_config_, fmt_, rtp_);

    sender_->init();
    receiver_->start();

    initialized_ = true;

    return ret;
}

rtp_error_t uvg_rtp::media_stream::add_srtp_ctx(uint8_t *key, uint8_t *salt)
{
    if (!key || !salt)
        return RTP_INVALID_VALUE;

    unsigned srtp_flags = RCE_SRTP | RCE_SRTP_KMNGMNT_USER;
    rtp_error_t ret     = RTP_OK;

    if ((flags_ & srtp_flags) != srtp_flags)
        return RTP_NOT_SUPPORTED;

    if (init_connection() != RTP_OK) {
        LOG_ERROR("Failed to initialize the underlying socket: %s!", strerror(errno));
        return RTP_GENERIC_ERROR;
    }

    if ((rtp_ = new uvg_rtp::rtp(fmt_)) == nullptr)
        return RTP_MEMORY_ERROR;

    if ((srtp_ = new uvg_rtp::srtp()) == nullptr)
        return RTP_MEMORY_ERROR;

    if ((ret = srtp_->init_user(SRTP, key, salt)) != RTP_OK) {
        LOG_WARN("Failed to initialize SRTP for media stream!");
        return ret;
    }

    socket_.set_srtp(srtp_);

    sender_   = new uvg_rtp::sender(socket_, ctx_config_, fmt_, rtp_);
    receiver_ = new uvg_rtp::receiver(socket_, ctx_config_, fmt_, rtp_);

    sender_->init();
    receiver_->start();

    initialized_ = true;

    return ret;
}
#endif

rtp_error_t uvg_rtp::media_stream::push_frame(uint8_t *data, size_t data_len, int flags)
{
    if (!initialized_) {
        LOG_ERROR("RTP context has not been initialized fully, cannot continue!");
        return RTP_NOT_INITIALIZED;
    }

    return sender_->push_frame(data, data_len, flags);
}

rtp_error_t uvg_rtp::media_stream::push_frame(std::unique_ptr<uint8_t[]> data, size_t data_len, int flags)
{
    if (!initialized_) {
        LOG_ERROR("RTP context has not been initialized fully, cannot continue!");
        return RTP_NOT_INITIALIZED;
    }

    return sender_->push_frame(std::move(data), data_len, flags);
}

rtp_error_t uvg_rtp::media_stream::push_frame(uint8_t *data, size_t data_len, uint32_t ts, int flags)
{
    rtp_error_t ret = RTP_GENERIC_ERROR;

    if (!initialized_) {
        LOG_ERROR("RTP context has not been initialized fully, cannot continue!");
        return RTP_NOT_INITIALIZED;
    }

    rtp_->set_timestamp(ts);
    ret = sender_->push_frame(data, data_len, flags);
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
    ret = sender_->push_frame(std::move(data), data_len, flags);
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

    return receiver_->pull_frame();
}

uvg_rtp::frame::rtp_frame *uvg_rtp::media_stream::pull_frame(size_t timeout)
{
    if (!initialized_) {
        LOG_ERROR("RTP context has not been initialized fully, cannot continue!");
        rtp_errno = RTP_NOT_INITIALIZED;
        return nullptr;
    }

    return receiver_->pull_frame(timeout);
}

rtp_error_t uvg_rtp::media_stream::install_receive_hook(void *arg, void (*hook)(void *, uvg_rtp::frame::rtp_frame *))
{
    if (!initialized_) {
        LOG_ERROR("RTP context has not been initialized fully, cannot continue!");
        return RTP_NOT_INITIALIZED;
    }

    if (!hook)
        return RTP_INVALID_VALUE;

    receiver_->install_recv_hook(arg, hook);

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

    sender_->install_dealloc_hook(hook);

    return RTP_OK;
}

rtp_error_t uvg_rtp::media_stream::install_notify_hook(void *arg, void (*hook)(void *, int))
{
    if (!initialized_) {
        LOG_ERROR("RTP context has not been initialized fully, cannot continue!");
        return RTP_NOT_INITIALIZED;
    }

    if (!hook)
        return RTP_INVALID_VALUE;

    receiver_->install_notify_hook(arg, hook);

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

            int buf_size = value;
            if ((ret = socket_.setsockopt(SOL_SOCKET, SO_SNDBUF, (const char *)&buf_size, sizeof(int))) != RTP_OK)
                return ret;
        }
        break;

        case RCC_UDP_RCV_BUF_SIZE: {
            if (value <= 0)
                return RTP_INVALID_VALUE;

            int buf_size = value;
            if ((ret = socket_.setsockopt(SOL_SOCKET, SO_RCVBUF, (const char *)&buf_size, sizeof(int))) != RTP_OK)
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

rtp_error_t uvg_rtp::media_stream::create_rtcp(uint16_t src_port, uint16_t dst_port)
{
    if (!(rtcp_ = new uvg_rtp::rtcp(rtp_->get_ssrc(), false)))
        return RTP_MEMORY_ERROR;

    return rtcp_->add_participant(addr_, dst_port, src_port, rtp_->get_clock_rate());
}

rtp_error_t uvg_rtp::media_stream::install_rtcp_sender_hook(void (*hook)(uvg_rtp::frame::rtcp_sender_frame *))
{
    if (!rtcp_)
        return RTP_NOT_INITIALIZED;
    return rtcp_->install_sender_hook(hook);
}

rtp_error_t uvg_rtp::media_stream::install_rtcp_receiver_hook(void (*hook)(uvg_rtp::frame::rtcp_receiver_frame *))
{
    if (!rtcp_)
        return RTP_NOT_INITIALIZED;
    return rtcp_->install_receiver_hook(hook);
}

rtp_error_t uvg_rtp::media_stream::install_rtcp_sdes_hook(void (*hook)(uvg_rtp::frame::rtcp_sdes_frame *))
{
    if (!rtcp_)
        return RTP_NOT_INITIALIZED;
    return rtcp_->install_sdes_hook(hook);
}

rtp_error_t uvg_rtp::media_stream::install_rtcp_app_hook(void (*hook)(uvg_rtp::frame::rtcp_app_frame *))
{
    if (!rtcp_)
        return RTP_NOT_INITIALIZED;
    return rtcp_->install_app_hook(hook);
}
