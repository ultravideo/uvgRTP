#include "uvgrtp/context.hh"

#include "uvgrtp/version.hh"
#include "uvgrtp/session.hh"

#include "crypto.hh"
#include "debug.hh"
#include "hostname.hh"
#include "socketfactory.hh"

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <algorithm>

thread_local rtp_error_t rtp_errno;

static inline std::string generate_string(size_t length)
{
    auto randchar = []() -> char
        {
            const char charset[] =
                "0123456789"
                "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                "abcdefghijklmnopqrstuvwxyz";
            const size_t max_index = (sizeof(charset) - 1);
            return charset[rand() % max_index];
        };

    std::string str(length, 0);
    std::generate_n(str.begin(), length, randchar);
    return str;
}


class uvgrtp::context::context_impl
{
public:
    context_impl()
    {
        cname_ = generate_cname();
        sfp_ = std::make_shared<uvgrtp::socketfactory>(RCE_NO_FLAGS);
    }

    std::string& get_cname()
    {
        return cname_;
    }

    /* Generate CNAME for participant using host and login names */
    std::string generate_cname() const
    {
        std::string host = uvgrtp::hostname::get_hostname();
        std::string user = uvgrtp::hostname::get_username();

        if (host == "")
            host = generate_string(10);

        if (user == "")
            user = generate_string(10);

        return host + "@" + user;
    }

    /* CNAME is the same for all connections */
    std::string cname_;
    std::shared_ptr<uvgrtp::socketfactory> sfp_;

};

uvgrtp::context::context():
    pimpl_(new context_impl())
{
#if UVGRTP_EXTENDED_API
    UVG_LOG_INFO("uvgRTP version: %s", uvgrtp::get_version().c_str());
#endif


#ifdef _WIN32
    WSADATA wsd;
    int rc;

    if ((rc = WSAStartup(MAKEWORD(2, 2), &wsd)) != 0)
        log_platform_error("WSAStartup() failed");
#endif
}

uvgrtp::context::~context()
{
    delete pimpl_;
#ifdef _WIN32
    WSACleanup();
#endif
}


uvgrtp::session* uvgrtp::context::create_session(const char* address)
{
    if (!address || address[0] == '\0') {
        UVG_LOG_ERROR("Please specify the address you want to communicate with!");
        return nullptr;
    }

    return new uvgrtp::session(pimpl_->get_cname(), std::string(address), pimpl_->sfp_);
}


uvgrtp::session* uvgrtp::context::create_session(const char* local_address, const char* remote_address)
{
    const bool has_local = local_address && local_address[0] != '\0';
    const bool has_remote = remote_address && remote_address[0] != '\0';

    if (!has_local && !has_remote) {
        UVG_LOG_ERROR("Please specify at least one address for create_session");
        return nullptr;
    }


    std::string local_str = has_local ? std::string(local_address) : std::string("");
    std::string remote_str = has_remote ? std::string(remote_address) : std::string("");

    return new uvgrtp::session(pimpl_->get_cname(), remote_str, local_str, pimpl_->sfp_);
}


#if UVGRTP_EXTENDED_API
uvgrtp::session* uvgrtp::context::create_session(std::pair<std::string, std::string> addresses)
{
    return create_session(addresses.second, addresses.first);
}

uvgrtp::session *uvgrtp::context::create_session(std::string address)
{
    if (address == "")
    {
        UVG_LOG_ERROR("Please specify the address you want to communicate with!");
        return nullptr;
    }

    return new uvgrtp::session(pimpl_->get_cname(), address, pimpl_->sfp_);
}

uvgrtp::session* uvgrtp::context::create_session(std::string remote_addr, std::string local_addr)
{
    if (remote_addr == "" && local_addr == "")
    {
        UVG_LOG_ERROR("Please specify at least one address for create_session");
        return nullptr;
    }
    return new uvgrtp::session(pimpl_->get_cname(), remote_addr, local_addr, pimpl_->sfp_);
}
#endif

rtp_error_t uvgrtp::context::destroy_session(uvgrtp::session *session)
{
    if (!session)
        return RTP_INVALID_VALUE;

    delete session;

    return RTP_OK;
}

bool uvgrtp::context::crypto_enabled() const
{
    return uvgrtp::crypto::enabled();
}
