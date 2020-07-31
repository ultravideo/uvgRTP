#include "debug.hh"
#include "session.hh"

uvg_rtp::session::session(std::string addr):
#ifdef __RTP_CRYPTO__
    zrtp_(nullptr),
#endif
    addr_(addr),
    laddr_("")
{
}

uvg_rtp::session::session(std::string remote_addr, std::string local_addr):
    session(remote_addr)
{
    laddr_ = local_addr;
}

uvg_rtp::session::~session()
{
    for (auto&i : streams_) {
        (void)destroy_stream(i.second);
    }
    streams_.clear();

#ifdef __RTP_CRYPTO__
    delete zrtp_;
#endif
}

uvg_rtp::media_stream *uvg_rtp::session::create_stream(int r_port, int s_port, rtp_format_t fmt, int flags)
{
    uvg_rtp::media_stream *stream = nullptr;

    if (flags & RCE_SYSTEM_CALL_DISPATCHER) {
        LOG_ERROR("SCD is not supported!");
        rtp_errno = RTP_NOT_SUPPORTED;
        return nullptr;
    }

    if (laddr_ == "")
        stream = new uvg_rtp::media_stream(addr_, r_port, s_port, fmt, flags);
    else
        stream = new uvg_rtp::media_stream(addr_, laddr_, r_port, s_port, fmt, flags);

    if (!stream) {
        LOG_ERROR("Failed to create media stream for %s:%d -> %s:%d",
            (laddr_ == "") ? "0.0.0.0" : laddr_.c_str(), s_port, addr_.c_str(), r_port
        );
        return nullptr;
    }

#ifdef __RTP_CRYPTO__
    if (flags & RCE_SRTP) {
        if (flags & RCE_SRTP_KMNGMNT_ZRTP) {
            if ((zrtp_ = new uvg_rtp::zrtp()) == nullptr) {
                rtp_errno = RTP_MEMORY_ERROR;
                return nullptr;
            }

            if (stream->init(zrtp_) != RTP_OK) {
                LOG_ERROR("Failed to initialize media stream %s:%d/%d", addr_.c_str(), r_port, s_port);
                return nullptr;
            }
        } else if (flags & RCE_SRTP_KMNGMNT_USER) {
            LOG_DEBUG("SRTP with user-managed keys enabled, postpone initialization");
        } else {
            LOG_ERROR("SRTP key management scheme not specified!");
            rtp_errno = RTP_INVALID_VALUE;
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

rtp_error_t uvg_rtp::session::destroy_stream(uvg_rtp::media_stream *stream)
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

std::string& uvg_rtp::session::get_key()
{
    return addr_;
}
