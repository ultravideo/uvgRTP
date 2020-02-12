#include "session.hh"

kvz_rtp::session::session(std::string addr):
    zrtp_(),
    addr_(addr)
{
}

kvz_rtp::session::~session()
{
    /* TODO: free all resources */
}

kvz_rtp::media_stream *kvz_rtp::session::create_stream(int r_port, int s_port, rtp_format_t fmt, int flags)
{
    kvz_rtp::media_stream *stream = new kvz_rtp::media_stream(addr_, r_port, s_port, fmt, flags);

    int zrtp_flags = (RCE_SRTP | RCE_SRTP_KMNGMNT_ZRTP);

    if ((flags & zrtp_flags) == zrtp_flags) {
        if (stream->init(zrtp_) != RTP_OK) {
            LOG_ERROR("Failed to initialize media stream %s:%d/%d", addr_.c_str(), r_port, s_port);
            return nullptr;
        }
    } else {
        if (stream->init() != RTP_OK) {
            LOG_ERROR("Failed to initialize media stream %s:%d/%d", addr_.c_str(), r_port, s_port);
            return nullptr;
        }
    }

    return stream;
}

rtp_error_t kvz_rtp::session::destroy_media_stream(kvz_rtp::media_stream *stream)
{
    (void)stream;

    return RTP_OK;
}
