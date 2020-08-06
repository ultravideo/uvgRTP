#include <cstdlib>
#include <cstring>
#include <iostream>

#include "debug.hh"
#include "hostname.hh"
#include "lib.hh"
#include "random.hh"

thread_local rtp_error_t rtp_errno;

uvg_rtp::context::context()
{
    cname_  = uvg_rtp::context::generate_cname();

#ifdef _WIN32
    WSADATA wsd;
    int rc;

    if ((rc = WSAStartup(MAKEWORD(2, 2), &wsd)) != 0)
        log_platform_error("WSAStartup() failed");
#endif

    uvg_rtp::random::init();
}

uvg_rtp::context::~context()
{
#ifdef _WIN32
    WSACleanup();
#endif
}

uvg_rtp::session *uvg_rtp::context::create_session(std::string address)
{
    if (address == "")
        return nullptr;

    return new uvg_rtp::session(address);
}

uvg_rtp::session *uvg_rtp::context::create_session(std::string remote_addr, std::string local_addr)
{
    if (remote_addr == "" || local_addr == "")
        return nullptr;

    return new uvg_rtp::session(remote_addr, local_addr);
}

rtp_error_t uvg_rtp::context::destroy_session(uvg_rtp::session *session)
{
    if (!session)
        return RTP_INVALID_VALUE;

    delete session;

    return RTP_OK;
}

std::string uvg_rtp::context::generate_cname()
{
    std::string host = uvg_rtp::hostname::get_hostname();
    std::string user = uvg_rtp::hostname::get_username();

    if (host == "")
        host = generate_string(10);

    if (user == "")
        user = generate_string(10);

    return host + "@" + user;
}

std::string& uvg_rtp::context::get_cname()
{
    return cname_;
}
