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

uvgrtp::context::context()
{
    UVG_LOG_INFO("uvgRTP version: %s", uvgrtp::get_version().c_str());

    cname_  = uvgrtp::context::generate_cname();
    sfp_ = std::make_shared<uvgrtp::socketfactory>(RCE_NO_FLAGS);

#ifdef _WIN32
    WSADATA wsd;
    int rc;

    if ((rc = WSAStartup(MAKEWORD(2, 2), &wsd)) != 0)
        log_platform_error("WSAStartup() failed");
#endif
}

uvgrtp::context::~context()
{
#ifdef _WIN32
    WSACleanup();
#endif
}

uvgrtp::session *uvgrtp::context::create_session(std::string address)
{
    if (address == "")
    {
        UVG_LOG_ERROR("Please specify the address you want to communicate with!");
        return nullptr;
    }

    return new uvgrtp::session(get_cname(), address, sfp_);
}

uvgrtp::session* uvgrtp::context::create_session(std::string remote_addr, std::string local_addr)
{
    if (remote_addr == "" && local_addr == "")
    {
        UVG_LOG_ERROR("Please specify at least one address for create_session");
        return nullptr;
    }
    return new uvgrtp::session(get_cname(), remote_addr, local_addr, sfp_);
}

rtp_error_t uvgrtp::context::destroy_session(uvgrtp::session *session)
{
    if (!session)
        return RTP_INVALID_VALUE;

    delete session;

    return RTP_OK;
}

std::string uvgrtp::context::generate_cname() const
{
    std::string host = uvgrtp::hostname::get_hostname();
    std::string user = uvgrtp::hostname::get_username();

    if (host == "")
        host = generate_string(10);

    if (user == "")
        user = generate_string(10);

    return host + "@" + user;
}

std::string& uvgrtp::context::get_cname()
{
    return cname_;
}

bool uvgrtp::context::crypto_enabled() const
{
    return uvgrtp::crypto::enabled();
}
