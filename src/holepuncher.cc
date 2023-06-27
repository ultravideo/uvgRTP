#include "holepuncher.hh"

#include "uvgrtp/clock.hh"

#include "socket.hh"
#include "debug.hh"


#define THRESHOLD 2000

uvgrtp::holepuncher::holepuncher(std::shared_ptr<uvgrtp::socket> socket):
    socket_(socket),
    last_dgram_sent_(0),
    remote_sockaddr_({}),
    remote_sockaddr_ip6_({}),
    active_(false)
{}

uvgrtp::holepuncher::~holepuncher()
{
    stop();
}

rtp_error_t uvgrtp::holepuncher::start()
{
    active_ = true;
    runner_ = std::unique_ptr<std::thread> (new std::thread(&uvgrtp::holepuncher::keepalive, this));
    return RTP_OK;
}

rtp_error_t uvgrtp::holepuncher::stop()
{
    active_ = false;
    if (runner_ && runner_->joinable())
    {
        runner_->join();
    }
    return RTP_OK;
}

rtp_error_t uvgrtp::holepuncher::set_remote_address(sockaddr_in& addr, sockaddr_in6& addr6)
{
    remote_sockaddr_ = addr;
    remote_sockaddr_ip6_ = addr6;
    return RTP_OK;
}


void uvgrtp::holepuncher::notify()
{
    last_dgram_sent_ = uvgrtp::clock::ntp::now();
}

void uvgrtp::holepuncher::keepalive()
{
    UVG_LOG_DEBUG("Starting holepuncher");

    /* RFC 6263 https://datatracker.ietf.org/doc/html/rfc6263
     * The RFC above describes several methods of implementing keep-alive. One of them (described in section 4.1) is
     * sending empty (0-Byte) packets, which is implemented here.
     * Another method (section 4.3) is multiplexing RTCP and RTP packets into a single socket, which keeps the connection
     * alive at all times with RTCP packets. This will be implemented into uvgRTP in the future. */
    while (active_) {
        if (uvgrtp::clock::ntp::diff_now(last_dgram_sent_) < THRESHOLD) {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            continue;
        }

        UVG_LOG_DEBUG("Sending keep-alive");
        uint8_t payload = 0b11000000;
        socket_->sendto(remote_sockaddr_, remote_sockaddr_ip6_, &payload, 1, 0);
        last_dgram_sent_ = uvgrtp::clock::ntp::now();
    }
    UVG_LOG_DEBUG("Stopping holepuncher");
}
