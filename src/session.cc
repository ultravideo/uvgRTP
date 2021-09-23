#include "session.hh"

#include "media_stream.hh"
#include "zrtp.hh"
#include "crypto.hh"
#include "debug.hh"


uvgrtp::session::session(std::string addr):
#ifdef __RTP_CRYPTO__
    zrtp_(nullptr),
#endif
    addr_(addr),
    laddr_("")
{
}

uvgrtp::session::session(std::string remote_addr, std::string local_addr):
    session(remote_addr)
{
    laddr_ = local_addr;
}

uvgrtp::session::~session()
{
    for (auto&i : streams_) {
        (void)destroy_stream(i.second);
    }
    streams_.clear();

#ifdef __RTP_CRYPTO__
    delete zrtp_;
#endif
}

uvgrtp::media_stream *uvgrtp::session::create_stream(int r_port, int s_port, rtp_format_t fmt, int flags)
{
    std::lock_guard<std::mutex> m(session_mtx_);

    uvgrtp::media_stream *stream = nullptr;

    if (flags & RCE_SYSTEM_CALL_DISPATCHER) {
        LOG_ERROR("SCD is not supported!");
        rtp_errno = RTP_NOT_SUPPORTED;
        return nullptr;
    }

    if (laddr_ == "")
        stream = new uvgrtp::media_stream(addr_, r_port, s_port, fmt, flags);
    else
        stream = new uvgrtp::media_stream(addr_, laddr_, r_port, s_port, fmt, flags);

    if (flags & RCE_SRTP) {
        if (!uvgrtp::crypto::enabled()) {
            LOG_ERROR("Recompile uvgRTP with -D__RTP_CRYPTO__");
            rtp_errno = RTP_GENERIC_ERROR;
            return nullptr;
        }

        if (flags & RCE_SRTP_REPLAY_PROTECTION)
            flags |= RCE_SRTP_AUTHENTICATE_RTP;

        if (flags & RCE_SRTP_KMNGMNT_ZRTP) {

            if (flags & (RCE_SRTP_KEYSIZE_192 | RCE_SRTP_KEYSIZE_256)) {
                LOG_ERROR("Only 128-bit keys are supported with ZRTP");
                return nullptr;
            }

            if (!zrtp_) {
                zrtp_ = new uvgrtp::zrtp();
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
        if (stream->init() != RTP_OK) {
            LOG_ERROR("Failed to initialize media stream %s:%d/%d", addr_.c_str(), r_port, s_port);
            return nullptr;
        }
    }

    streams_.insert(std::make_pair(stream->get_key(), stream));

    return stream;
}

rtp_error_t uvgrtp::session::destroy_stream(uvgrtp::media_stream *stream)
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

std::string& uvgrtp::session::get_key()
{
    return addr_;
}
