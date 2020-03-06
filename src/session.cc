#include "debug.hh"
#include "session.hh"

kvz_rtp::session::session(std::string addr):
#ifdef __RTP_CRYPTO__
    zrtp_(nullptr),
#endif
    addr_(addr)
{
}

kvz_rtp::session::~session()
{
    for (auto&i : streams_) {
        (void)destroy_stream(i.second);
    }
    streams_.clear();

#ifdef __RTP_CRYPTO__
    delete zrtp_;
#endif
}

kvz_rtp::media_stream *kvz_rtp::session::create_stream(int r_port, int s_port, rtp_format_t fmt, int flags)
{
    kvz_rtp::media_stream *stream = new kvz_rtp::media_stream(addr_, r_port, s_port, fmt, flags);

#ifdef __RTP_CRYPTO__
    int zrtp_flags = (RCE_SRTP | RCE_SRTP_KMNGMNT_ZRTP);

    if ((flags & zrtp_flags) == zrtp_flags) {
        if ((zrtp_ = new kvz_rtp::zrtp()) == nullptr) {
            rtp_errno = RTP_MEMORY_ERROR;
            return nullptr;
        }

        if (stream->init(zrtp_) != RTP_OK) {
            LOG_ERROR("Failed to initialize media stream %s:%d/%d", addr_.c_str(), r_port, s_port);
            return nullptr;
        }
    } else {
#endif
        if (stream->init() != RTP_OK) {
            LOG_ERROR("Failed to initialize media stream %s:%d/%d", addr_.c_str(), r_port, s_port);
            return nullptr;
        }

#ifdef __RTP_CRYPTO__
    }
#endif

    streams_.insert(std::make_pair(stream->get_key(), stream));

    return stream;
}

rtp_error_t kvz_rtp::session::destroy_stream(kvz_rtp::media_stream *stream)
{
    if (!stream)
        return RTP_INVALID_VALUE;

    auto mstream = streams_.find(stream->get_key());

    if (mstream == streams_.end())
        return RTP_NOT_FOUND;

    delete mstream->second;
    mstream->second = nullptr;

    return RTP_OK;
}

std::string& kvz_rtp::session::get_key()
{
    return addr_;
}
