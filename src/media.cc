#ifdef __linux__
#else
#endif

#include "media.hh"

uvg_rtp::formats::media::media(uvg_rtp::socket *socket, uvg_rtp::rtp *rtp_ctx, int flags):
    socket_(socket), rtp_ctx_(rtp_ctx), flags_(flags)
{
}

uvg_rtp::formats::media::~media()
{
}

rtp_error_t uvg_rtp::formats::media::push_frame(uint8_t *data, size_t data_len, int flags)
{
    if (!data || !data_len)
        return RTP_INVALID_VALUE;

    return __push_frame(data, data_len, flags);
}

rtp_error_t uvg_rtp::formats::media::push_frame(std::unique_ptr<uint8_t[]> data, size_t data_len, int flags)
{
    if (!data || !data_len)
        return RTP_INVALID_VALUE;

    return __push_frame(data.get(), data_len, flags);
}

rtp_error_t uvg_rtp::formats::media::__push_frame(uint8_t *data, size_t data_len, int flags)
{
    (void)data, (void)data_len, (void)flags;
    return RTP_GENERIC_ERROR;
}

static rtp_error_t packet_handler(ssize_t size, void *packet)
{
    (void)size, (void)packet;
    return RTP_OK;
}
