#include "lib.hh"

#include "debug.hh"
#include "hostname.hh"
#include "random.hh"
#include "session.hh"
#include "version.hh"

#include <cstdlib>
#include <cstring>
#include <iostream>

thread_local rtp_error_t rtp_errno;

uvgrtp::context::context()
{
    LOG_INFO("uvgRTP version: %s", uvgrtp::get_version().c_str());

    cname_  = uvgrtp::context::generate_cname();

#ifdef _WIN32
    WSADATA wsd;
    int rc;

    if ((rc = WSAStartup(MAKEWORD(2, 2), &wsd)) != 0)
        log_platform_error("WSAStartup() failed");
#endif

    uvgrtp::random::init();
}

uvgrtp::context::~context()
{
#ifdef _WIN32
    WSACleanup();
#endif
}

uvgrtp::session *uvgrtp::context::create_session(std::string remote_addr)
{
    if (remote_addr == "")
        return nullptr;

    return new uvgrtp::session(remote_addr);
}

uvgrtp::session *uvgrtp::context::create_session(std::string remote_addr, std::string local_addr)
{
    if (remote_addr == "" || local_addr == "")
        return nullptr;

    return new uvgrtp::session(remote_addr, local_addr);
}

rtp_error_t uvgrtp::context::destroy_session(uvgrtp::session *session)
{
    if (!session)
        return RTP_INVALID_VALUE;

    delete session;

    return RTP_OK;
}

std::string uvgrtp::context::generate_cname()
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
