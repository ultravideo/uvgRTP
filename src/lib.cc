#include <cstdlib>
#include <cstring>
#include <iostream>

#include "debug.hh"
#include "hostname.hh"
#include "lib.hh"
#include "random.hh"

thread_local rtp_error_t rtp_errno;

kvz_rtp::context::context()
{
    cname_  = kvz_rtp::context::generate_cname();

#ifdef _WIN32
    WSADATA wsd;
    int rc;

    if ((rc = WSAStartup(MAKEWORD(2, 2), &wsd)) != 0) {
        win_get_last_error();
    }
#endif

    kvz_rtp::random::init();
}

kvz_rtp::context::~context()
{
#ifdef _WIN32
    WSACleanup();
#endif
}

kvz_rtp::session *kvz_rtp::context::create_session(std::string address)
{
    if (address == "")
        return nullptr;

    return new kvz_rtp::session(address);
}

rtp_error_t kvz_rtp::context::destroy_session(kvz_rtp::session *session)
{
    if (!session)
        return RTP_INVALID_VALUE;

    delete session;

    return RTP_OK;
}

std::string kvz_rtp::context::generate_cname()
{
    std::string host = kvz_rtp::hostname::get_hostname();
    std::string user = kvz_rtp::hostname::get_username();

    if (host == "")
        host = generate_string(10);

    if (user == "")
        user = generate_string(10);

    return host + "@" + user;
}

std::string& kvz_rtp::context::get_cname()
{
    return cname_;
}
