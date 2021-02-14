#include "clock.hh"
#include "debug.hh"
#include "holepuncher.hh"

#define THRESHOLD 2000

uvg_rtp::holepuncher::holepuncher(uvg_rtp::socket *socket):
    socket_(socket),
    last_dgram_sent_(0)
{
}

uvg_rtp::holepuncher::~holepuncher()
{
}

rtp_error_t uvg_rtp::holepuncher::start()
{
    if (!(runner_ = new std::thread(&uvg_rtp::holepuncher::keepalive, this)))
        return RTP_MEMORY_ERROR;

    runner_->detach();
    return uvg_rtp::runner::start();
}

rtp_error_t uvg_rtp::holepuncher::stop()
{
    return uvg_rtp::runner::stop(); 
}

void uvg_rtp::holepuncher::notify()
{
    last_dgram_sent_ = uvg_rtp::clock::ntp::now();
}

void uvg_rtp::holepuncher::keepalive()
{
    while (active()) {
        if (!last_dgram_sent_ || uvg_rtp::clock::ntp::diff_now(last_dgram_sent_) < THRESHOLD) {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            continue;
        }

        uint8_t payload = 0x00;
        socket_->sendto(&payload, 1, 0);
        last_dgram_sent_ = uvg_rtp::clock::ntp::now();
    }
}
