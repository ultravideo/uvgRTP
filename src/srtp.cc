#include "srtp.hh"

kvz_rtp::srtp::srtp(int type):
    zrtp_(nullptr),
    s_ctx(nullptr)
{
    (void)type;
}

kvz_rtp::srtp::~srtp()
{
    delete zrtp_;
}

rtp_error_t kvz_rtp::srtp::init_zrtp(uint32_t ssrc, socket_t& socket, sockaddr_in& addr)
{
    LOG_INFO("Begin SRTP initialization procedure...");

    (void)socket, (void)ssrc;

    if ((zrtp_ = new kvz_rtp::zrtp()) == nullptr) {
        LOG_ERROR("Failed to allocate ZRTP context");
        return RTP_MEMORY_ERROR;
    }

    LOG_DEBUG("Begin ZRTP initialization procedure...");

    return zrtp_->init(ssrc, socket, addr);
}

rtp_error_t kvz_rtp::srtp::init_user(uint32_t ssrc, std::pair<uint8_t *, size_t>& key)
{
    (void)ssrc, (void)key;

    return RTP_OK;
}

rtp_error_t kvz_rtp::srtp::encrypt(uint8_t *buf, size_t len)
{
    (void)buf, (void)len;

    if (!zrtp_)
        return RTP_NOT_INITIALIZED;

    return RTP_OK;
}

rtp_error_t kvz_rtp::srtp::decrypt(uint8_t *buf, size_t len)
{
    (void)buf, (void)len;

    if (!zrtp_)
        return RTP_NOT_INITIALIZED;

    return RTP_OK;
}
