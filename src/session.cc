#include "uvgrtp/session.hh"

#include "uvgrtp/media_stream.hh"

#include "media_stream_internal.hh"
#include "socketfactory.hh"
#include "crypto.hh"
#include "zrtp.hh"
#include "debug.hh"

namespace uvgrtp {

    class session_impl {
    public:
        session_impl(std::string cname, std::string generic_addr,
            std::string remote_addr, std::string local_addr, std::shared_ptr<uvgrtp::socketfactory> sfp) :
#ifdef __RTP_CRYPTO__
            zrtp_(new uvgrtp::zrtp()),
#endif
            generic_address_(generic_addr),
            remote_address_(remote_addr),
            local_address_(local_addr),
            cname_(cname),
            sf_(sfp)
        {
            if (generic_addr != "")
            {
                sf_->set_local_interface(generic_addr);
            }
            else
            {
                sf_->set_local_interface(local_addr);
            }
        }

        /* Each RTP multimedia session shall have one ZRTP session from which all session are derived */
        std::shared_ptr<uvgrtp::zrtp> zrtp_;

        std::string generic_address_;

        /* Each RTP multimedia session is always IP-specific */
        std::string remote_address_;

        /* If user so wishes, the session can be bound to a certain interface */
        std::string local_address_;

        /* All media streams of this session */
        std::unordered_map<uint32_t, uvgrtp::media_stream*> streams_;

        std::mutex session_mtx_;

        std::string cname_;
        std::shared_ptr<uvgrtp::socketfactory> sf_;
    };
}


uvgrtp::session::session(std::string cname, std::string addr, std::shared_ptr<uvgrtp::socketfactory> sfp) :
    pimpl_(std::make_unique<session_impl>(std::move(cname), std::move(addr), "", "", std::move(sfp)))
{}

uvgrtp::session::session(std::string cname, std::string remote_addr, std::string local_addr, 
    std::shared_ptr<uvgrtp::socketfactory> sfp)
    :pimpl_(std::make_unique<session_impl>(std::move(cname), "", std::move(remote_addr), std::move(local_addr), std::move(sfp)))
{}

uvgrtp::session::~session()
{
    for (auto& i : pimpl_->streams_) {
        (void)destroy_stream(i.second);
    }
    pimpl_->streams_.clear();
    pimpl_->sf_ = nullptr;
}

uvgrtp::media_stream* uvgrtp::session::create_stream(uint16_t port, rtp_format_t fmt, int rce_flags)
{
    if (rce_flags & RCE_RECEIVE_ONLY)
    {
        return create_stream(port, 0, fmt, rce_flags);
    }
    else if (rce_flags & RCE_SEND_ONLY)
    {
        return create_stream(0, port, fmt, rce_flags);
    }
    
    UVG_LOG_WARN("You haven't specified the purpose of port with rce_flags. Using it as destination port and not binding");
    return create_stream(0, port, fmt, rce_flags);
}

uvgrtp::media_stream* uvgrtp::session::create_stream(uint16_t src_port, uint16_t dst_port, rtp_format_t fmt, int rce_flags)
{
    if (rce_flags & RCE_OBSOLETE) {
        UVG_LOG_WARN("You are using a flag that has either been removed or has been enabled by default. Consider updating RCE flags");
    }

    if ((rce_flags & RCE_SEND_ONLY) && (rce_flags & RCE_RECEIVE_ONLY)) {
        UVG_LOG_ERROR("Cannot both use RCE_SEND_ONLY and RCE_RECEIVE_ONLY!");
        rtp_errno = RTP_NOT_SUPPORTED;
        return nullptr;
    }

    // select which address the one address we got as a parameter is
    if (pimpl_->generic_address_ != "")
    {
        if (rce_flags & RCE_RECEIVE_ONLY)
        {
            pimpl_->local_address_ = pimpl_->generic_address_;
        }
        else
        {
            pimpl_->remote_address_ = pimpl_->generic_address_;
        }
    }
    else if ((rce_flags & RCE_RECEIVE_ONLY) && pimpl_->local_address_ == "")
    {
        UVG_LOG_ERROR("RCE_RECEIVE_ONLY requires local address!");
        rtp_errno = RTP_INVALID_VALUE;
        return  nullptr;
    }
    else if ((rce_flags & RCE_SEND_ONLY) && pimpl_->remote_address_ == "")
    {
        UVG_LOG_ERROR("RCE_SEND_ONLY requires remote address!");
        rtp_errno = RTP_INVALID_VALUE;
        return  nullptr;
    }

    if ((rce_flags & RCE_RECEIVE_ONLY) && src_port == 0)
    {
        UVG_LOG_ERROR("RCE_RECEIVE_ONLY requires source port!");
        rtp_errno = RTP_INVALID_VALUE;
        return  nullptr;
    }

    if ((rce_flags & RCE_SEND_ONLY) && dst_port == 0)
    {
        UVG_LOG_ERROR("RCE_SEND_ONLY requires destination port!");
        rtp_errno = RTP_INVALID_VALUE;
        return  nullptr;
    }

    if (rce_flags & RCE_SRTP && !uvgrtp::crypto::enabled()) 
    {
        UVG_LOG_ERROR("RCE_SRTP requires inclusion of Crypto++ during compilation!");
        rtp_errno = RTP_GENERIC_ERROR;
        return nullptr;
    }
    
    uvgrtp::media_stream* stream =
        new uvgrtp::media_stream(pimpl_->cname_, pimpl_->remote_address_, pimpl_->local_address_, src_port, dst_port, fmt, pimpl_->sf_, rce_flags);

    if (rce_flags & RCE_SRTP) {

        pimpl_->session_mtx_.lock();
        if (!pimpl_->zrtp_) {
            pimpl_->zrtp_ = std::shared_ptr<uvgrtp::zrtp>(new uvgrtp::zrtp());
        }
        pimpl_->session_mtx_.unlock();

        if (rce_flags & RCE_SRTP_REPLAY_PROTECTION)
            rce_flags |= RCE_SRTP_AUTHENTICATE_RTP;

        /* With flags RCE_SRTP_KMNGMNT_ZRTP enabled, start ZRTP negotiation automatically.  NOTE! This only works when
         * not doing socket multiplexing. 
         * 
         * More info on flags: When using ZRTP, you have the following options:
         * 1. Use flags RCE_SRTP + RCE_SRTP_KMNGMNT_ZRTP + negotiation mode flag
         *     -> This way ZRTP negotiation is started automatically
         * 2. Use flags RCE_SRTP + negotiation mode flag
         *     -> Use start_zrtp() function to start ZRTP negotiation manually
         */
        if (rce_flags & RCE_SRTP_KMNGMNT_ZRTP) {

            if (rce_flags & (RCE_SRTP_KEYSIZE_192 | RCE_SRTP_KEYSIZE_256)) {
                UVG_LOG_ERROR("Only 128-bit keys are supported with ZRTP");
                delete stream;
                return nullptr;
            }

            if (!(rce_flags & RCE_ZRTP_DIFFIE_HELLMAN_MODE) &&
                !(rce_flags & RCE_ZRTP_MULTISTREAM_MODE)) {
                UVG_LOG_INFO("ZRTP mode not selected, using Diffie-Hellman mode");
                rce_flags |= RCE_ZRTP_DIFFIE_HELLMAN_MODE;
            }

            if (stream->impl_->init_auto_zrtp(pimpl_->zrtp_) != RTP_OK) {
                UVG_LOG_ERROR("Failed to initialize media stream %s:%d/%d", pimpl_->remote_address_.c_str(), src_port, dst_port);
                delete stream;
                return nullptr;
            }
        } else if (rce_flags & RCE_SRTP_KMNGMNT_USER) {
            UVG_LOG_DEBUG("SRTP with user-managed keys enabled, postpone initialization");
            if (stream->impl_->init(pimpl_->zrtp_) != RTP_OK) {
                UVG_LOG_ERROR("Failed to initialize media stream %s:%d/%d", pimpl_->remote_address_.c_str(), src_port, dst_port);
                delete stream;
                return nullptr;
            }
        } else {
            UVG_LOG_DEBUG("SRTP key management scheme not specified. Assuming key management will be determined later");
            if (stream->impl_->init(pimpl_->zrtp_) != RTP_OK) {
                UVG_LOG_ERROR("Failed to initialize media stream %s:%d/%d", pimpl_->remote_address_.c_str(), src_port, dst_port);
                delete stream;
                return nullptr;
            }
        }
    } else {
        if (stream->impl_->init(pimpl_->zrtp_) != RTP_OK) {
            UVG_LOG_ERROR("Failed to initialize media stream %s:%d/%d", pimpl_->remote_address_.c_str(), src_port, dst_port);
            delete stream;
            return nullptr;
        }
    }

    pimpl_->session_mtx_.lock();
    pimpl_->streams_.insert(std::make_pair(stream->impl_->get_key(), stream));
    pimpl_->session_mtx_.unlock();

    return stream;
}

rtp_error_t uvgrtp::session::destroy_stream(uvgrtp::media_stream *stream)
{
    if (!stream)
        return RTP_INVALID_VALUE;

    auto mstream = pimpl_->streams_.find(stream->impl_->get_key());

    if (mstream == pimpl_->streams_.end())
        return RTP_NOT_FOUND;

    delete mstream->second;
    mstream->second = nullptr;

    return RTP_OK;
}

std::string& uvgrtp::session::get_key()
{
    return pimpl_->remote_address_;
}
