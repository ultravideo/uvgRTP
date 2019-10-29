#include "runner.hh"

kvz_rtp::runner::runner():
    active_(false), runner_(nullptr)
{
}

kvz_rtp::runner::~runner()
{
    active_ = false;

    if (runner_)
        delete runner_;
}

rtp_error_t kvz_rtp::runner::start()
{
    active_ = true;

    return RTP_OK;
}

rtp_error_t kvz_rtp::runner::stop()
{
    active_ = false;

    return RTP_OK;
}

bool kvz_rtp::runner::active()
{
    return active_;
}
