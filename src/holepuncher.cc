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
    active_.store(true);
    runner_ = std::unique_ptr<std::thread>(new std::thread(&uvgrtp::holepuncher::keepalive, this));
    return RTP_OK;
}

rtp_error_t uvgrtp::holepuncher::stop()
{
    active_.store(false);
    cv_.notify_all();
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
    // I think this prevents us from sending packets when we don't need to
    last_dgram_sent_ = uvgrtp::clock::ntp::now();
}

void uvgrtp::holepuncher::keepalive()
{
    UVG_LOG_DEBUG("Starting holepuncher");

    last_dgram_sent_ = uvgrtp::clock::ntp::now();

    /* RFC 6263 https://datatracker.ietf.org/doc/html/rfc6263
     * The RFC above describes several methods of implementing keep-alive. One of them (described in section 4.1) is
     * sending empty (0-Byte) packets, which is implemented here.
     * Another method (section 4.3) is multiplexing RTCP and RTP packets into a single socket, which keeps the connection
     * alive at all times with RTCP packets.  */
    std::unique_lock<std::mutex> lock(cv_mutex_);
    while (active_.load()) {
        if (uvgrtp::clock::ntp::diff_now(last_dgram_sent_) < THRESHOLD) {
            // Wake either on timeout or on stop() notifying cv_
            cv_.wait_for(lock, std::chrono::milliseconds(100), [this]() 
                { 
                    return !active_.load() || (uvgrtp::clock::ntp::diff_now(last_dgram_sent_) >= THRESHOLD); 
                });
            continue;
        }

        UVG_LOG_DEBUG("Sending keep-alive");
        uint8_t payload = 0b11000000;
        // Avoid holding cv_mutex_ while doing network I/O which may block
        lock.unlock();
        socket_->sendto(remote_sockaddr_, remote_sockaddr_ip6_, &payload, 1, 0);
        last_dgram_sent_ = uvgrtp::clock::ntp::now();
        lock.lock();
    }
    UVG_LOG_DEBUG("Stopping holepuncher");
}
