#include "holepuncher.hh"

#include "clock.hh"
#include "socket.hh"
#include "debug.hh"


#define THRESHOLD 2000

uvgrtp::holepuncher::holepuncher(uvgrtp::socket *socket):
    socket_(socket),
    last_dgram_sent_(0)
{
}

uvgrtp::holepuncher::~holepuncher()
{
}

rtp_error_t uvgrtp::holepuncher::start()
{
    runner_ = new std::thread(&uvgrtp::holepuncher::keepalive, this);
    runner_->detach();
    return uvgrtp::runner::start();
}

rtp_error_t uvgrtp::holepuncher::stop()
{
    return uvgrtp::runner::stop(); 
}

void uvgrtp::holepuncher::notify()
{
    last_dgram_sent_ = uvgrtp::clock::ntp::now();
}

void uvgrtp::holepuncher::keepalive()
{
    while (active()) {
        if (uvgrtp::clock::ntp::diff_now(last_dgram_sent_) < THRESHOLD) {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            continue;
        }

        uint8_t payload = 0x00;
        socket_->sendto(&payload, 1, 0);
        last_dgram_sent_ = uvgrtp::clock::ntp::now();
    }
}
