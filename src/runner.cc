#include "uvgrtp/runner.hh"

uvgrtp::runner::runner():
    active_(false), runner_(nullptr)
{
}

uvgrtp::runner::~runner()
{
    active_ = false;

    if (runner_)
        delete runner_;
}

rtp_error_t uvgrtp::runner::start()
{
    active_ = true;

    return RTP_OK;
}

rtp_error_t uvgrtp::runner::stop()
{
    active_ = false;

    return RTP_OK;
}

bool uvgrtp::runner::active()
{
    return active_;
}
