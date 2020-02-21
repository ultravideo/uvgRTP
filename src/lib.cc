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
    for (auto& session : sessions_) {
        (void)destroy_session(session.second);
    }
    sessions_.clear();

#ifdef _WIN32
    WSACleanup();
#endif
}

kvz_rtp::session *kvz_rtp::context::create_session(std::string address)
{
    auto sess_it = sessions_.find(address);

    if (sess_it != sessions_.end())
        return sess_it->second;

    auto session = new kvz_rtp::session(address);
    sessions_.insert(std::make_pair(address, session));

    return session;
}

rtp_error_t kvz_rtp::context::destroy_session(kvz_rtp::session *session)
{
    if (!session)
        return RTP_INVALID_VALUE;

    auto session_it = sessions_.find(session->get_key());

    if (session_it == sessions_.end())
        return RTP_NOT_FOUND;

    delete session_it->second;
    session_it->second = nullptr;

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
