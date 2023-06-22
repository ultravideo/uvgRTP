#include "reception_flow.hh"

#include "uvgrtp/util.hh"
#include "uvgrtp/frame.hh"

#include "socket.hh"
#include "debug.hh"
#include "random.hh"
#include "uvgrtp/rtcp.hh"

#include "global.hh"

#include <chrono>

#ifndef _WIN32
#include <errno.h>
#include <poll.h>
#include <pthread.h>
#else
#define MSG_DONTWAIT 0
#endif

#include <cstring>
#ifdef _WIN32
#include <ws2ipdef.h>
#else
#include <netinet/ip.h>
#include <sys/socket.h>
#endif

constexpr size_t DEFAULT_INITIAL_BUFFER_SIZE = 4194304;

uvgrtp::reception_flow::reception_flow(bool ipv6) :
    frames_({}),
    hooks_({}),
    should_stop_(true),
    receiver_(nullptr),
    //user_hook_arg_(nullptr),
    //user_hook_(nullptr),
    NEW_packet_handlers_({}),
    ring_buffer_(),
    ring_read_index_(-1), // invalid first index that will increase to a valid one
    last_ring_write_index_(-1),
    socket_(),
    buffer_size_kbytes_(DEFAULT_INITIAL_BUFFER_SIZE),
    payload_size_(MAX_IPV4_PAYLOAD),
    active_(false),
    ipv6_(ipv6)
{
    create_ring_buffer();
}

uvgrtp::reception_flow::~reception_flow()
{
    hooks_.clear();
    destroy_ring_buffer();
    clear_frames();
}

void uvgrtp::reception_flow::clear_frames()
{
    frames_mtx_.lock();
    for (auto& frame : frames_)
    {
        (void)uvgrtp::frame::dealloc_frame(frame);
    }

    frames_.clear();
    frames_mtx_.unlock();
}

void uvgrtp::reception_flow::create_ring_buffer()
{
    destroy_ring_buffer();
    size_t elements = buffer_size_kbytes_ / payload_size_;

    for (size_t i = 0; i < elements; ++i)
    {
        uint8_t* data = new uint8_t[payload_size_];
        if (data)
        {
            ring_buffer_.push_back({ data, 0, {}, {} });
        }
        else
        {
            UVG_LOG_ERROR("Failed to allocate memory for ring buffer");
        }
    }
}

void uvgrtp::reception_flow::destroy_ring_buffer()
{
    for (size_t i = 0; i < ring_buffer_.size(); ++i)
    {
        if (ring_buffer_.at(i).data)
        {
            delete[] ring_buffer_.at(i).data;
        }
    }
    ring_buffer_.clear();
}

void uvgrtp::reception_flow::set_buffer_size(const ssize_t& value)
{
    buffer_size_kbytes_ = value;
    create_ring_buffer();
}

ssize_t uvgrtp::reception_flow::get_buffer_size() const
{
    return buffer_size_kbytes_;
}

void uvgrtp::reception_flow::set_payload_size(const size_t& value)
{
    payload_size_ = value;
    create_ring_buffer();
}

rtp_error_t uvgrtp::reception_flow::start(std::shared_ptr<uvgrtp::socket> socket, int rce_flags)
{
    if (active_) {
        return RTP_OK;
    }
    should_stop_ = false;

    UVG_LOG_DEBUG("Creating receiving threads and setting priorities");
    processor_ = std::unique_ptr<std::thread>(new std::thread(&uvgrtp::reception_flow::process_packet, this, rce_flags));
    receiver_ = std::unique_ptr<std::thread>(new std::thread(&uvgrtp::reception_flow::receiver, this, socket));

    // set receiver thread priority to maximum
#ifndef WIN32
    struct sched_param params;
    params.sched_priority = sched_get_priority_max(SCHED_FIFO);
    pthread_setschedparam(receiver_->native_handle(), SCHED_FIFO, &params);
    params.sched_priority = sched_get_priority_max(SCHED_FIFO) - 1;
    pthread_setschedparam(processor_->native_handle(), SCHED_FIFO, &params);
#else

    SetThreadPriority(receiver_->native_handle(), REALTIME_PRIORITY_CLASS);
    SetThreadPriority(processor_->native_handle(), ABOVE_NORMAL_PRIORITY_CLASS);

#endif
    active_ = true;
    return RTP_ERROR::RTP_OK;
}

rtp_error_t uvgrtp::reception_flow::stop()
{
    if (!active_) {
        return RTP_OK;
    }
    should_stop_ = true;
    process_cond_.notify_all();

    if (receiver_ != nullptr && receiver_->joinable())
    {
        receiver_->join();
    }

    if (processor_ != nullptr && processor_->joinable())
    {
        processor_->join();
    }

    clear_frames();
    active_ = false;

    return RTP_OK;
}

rtp_error_t uvgrtp::reception_flow::install_receive_hook(
    void *arg,
    void (*hook)(void *, uvgrtp::frame::rtp_frame *),
    std::shared_ptr<std::atomic<std::uint32_t>> remote_ssrc
)
{
    if (!hook)
        return RTP_INVALID_VALUE;

    // ssrc 0 is used when streams are not multiplexed into a single socket
    if (hooks_.find(remote_ssrc) == hooks_.end()) {
        receive_pkt_hook new_hook = { arg, hook };
        hooks_[remote_ssrc] = new_hook;
    }
    else {
        receive_pkt_hook new_hook = { arg, hook };
        hooks_.erase(remote_ssrc);
        hooks_.insert({remote_ssrc, new_hook});
    }

    return RTP_OK;
}

uvgrtp::frame::rtp_frame *uvgrtp::reception_flow::pull_frame()
{
    while (frames_.empty() && !should_stop_)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    if (should_stop_)
        return nullptr;

    frames_mtx_.lock();
    auto frame = frames_.front();
    frames_.erase(frames_.begin());
    frames_mtx_.unlock();

    return frame;
}

uvgrtp::frame::rtp_frame *uvgrtp::reception_flow::pull_frame(ssize_t timeout_ms)
{
    auto start_time = std::chrono::high_resolution_clock::now();

    while (frames_.empty() && 
        !should_stop_ &&
        timeout_ms > std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - start_time).count())
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    if (should_stop_ || frames_.empty())
        return nullptr;

    frames_mtx_.lock();
    auto frame = frames_.front();
    frames_.pop_front();
    frames_mtx_.unlock();

    return frame;
}

uvgrtp::frame::rtp_frame* uvgrtp::reception_flow::pull_frame(std::shared_ptr<std::atomic<std::uint32_t>> remote_ssrc)
{
    while (frames_.empty() && !should_stop_)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    if (should_stop_)
        return nullptr;

    // Check if the source ssrc in the frame matches the remote ssrc that we want to pull frames from
    bool found_frame = false;
    frames_mtx_.lock();
    auto frame = frames_.front();
    if (frame->header.ssrc == remote_ssrc.get()->load()) {
        frames_.erase(frames_.begin());
        found_frame = true;
    }
    frames_mtx_.unlock();
    if (found_frame) {
        return frame;
    }
    return nullptr;
}

uvgrtp::frame::rtp_frame* uvgrtp::reception_flow::pull_frame(ssize_t timeout_ms, std::shared_ptr<std::atomic<std::uint32_t>> remote_ssrc)
{
    auto start_time = std::chrono::high_resolution_clock::now();

    while (frames_.empty() &&
        !should_stop_ &&
        timeout_ms > std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - start_time).count())
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    if (should_stop_ || frames_.empty())
        return nullptr;
    // Check if the source ssrc in the frame matches the remote ssrc that we want to pull frames from
    bool found_frame = false;
    frames_mtx_.lock();
    auto frame = frames_.front();
    if (frame->header.ssrc == remote_ssrc.get()->load()) {
        frames_.pop_front();
        found_frame = true;
    }
    frames_mtx_.unlock();
    if (found_frame) {
        return frame;
    }
    return nullptr;
}

rtp_error_t uvgrtp::reception_flow::new_install_handler(int type, std::shared_ptr<std::atomic<std::uint32_t>> remote_ssrc,
    std::function<rtp_error_t(void*, int, uint8_t*, size_t, frame::rtp_frame** out)> handler, void* args)
{
    switch (type) {
        case 1: {
            NEW_packet_handlers_[remote_ssrc].rtp.handler = handler;
            NEW_packet_handlers_[remote_ssrc].rtp.args = args;
            break;
        }
        case 2: {
            NEW_packet_handlers_[remote_ssrc].rtcp.handler = handler;
            NEW_packet_handlers_[remote_ssrc].rtcp.args = args;
            break;
        }
        case 3: {
            NEW_packet_handlers_[remote_ssrc].zrtp.handler = handler;
            NEW_packet_handlers_[remote_ssrc].zrtp.args = args;
            break;
        }
        case 4: {
            NEW_packet_handlers_[remote_ssrc].srtp.handler = handler;
            NEW_packet_handlers_[remote_ssrc].srtp.args = args;
            break;
        }
        case 5: {
            NEW_packet_handlers_[remote_ssrc].media.handler = handler;
            NEW_packet_handlers_[remote_ssrc].media.args = args;
            break;
        }
        case 6: {
            NEW_packet_handlers_[remote_ssrc].rtcp_common.handler = handler;
            NEW_packet_handlers_[remote_ssrc].rtcp_common.args = args;
            break;
        }
        default: {
            UVG_LOG_ERROR("Invalid type, only types 1-5 are allowed");
            break;
        }
    }
    return RTP_OK;
}

rtp_error_t uvgrtp::reception_flow::new_install_getter(std::shared_ptr<std::atomic<std::uint32_t>> remote_ssrc,
    std::function<rtp_error_t(uvgrtp::frame::rtp_frame**)> getter)
{
    NEW_packet_handlers_[remote_ssrc].getter = getter;
    return RTP_OK;
}

rtp_error_t uvgrtp::reception_flow::new_remove_handlers(std::shared_ptr<std::atomic<std::uint32_t>> remote_ssrc)
{
    int removed = NEW_packet_handlers_.erase(remote_ssrc);
    if (removed == 1) {
        return RTP_OK;
    }
    return RTP_INVALID_VALUE;
}

void uvgrtp::reception_flow::return_frame(uvgrtp::frame::rtp_frame *frame)
{
    uint32_t ssrc = frame->header.ssrc;

    // 1. Check if there exists a hook that this ssrc belongs to
    // 2. If not, check if there is a "universal hook"
    // 3. If neither is found, push the frame to the queue

    bool found = false;
    for (auto it = hooks_.begin(); it != hooks_.end(); ++it) {
        if (it->first.get()->load() == ssrc) {
            receive_pkt_hook pkt_hook = it->second;
            recv_hook hook = pkt_hook.hook;
            void* arg = pkt_hook.arg;
            hook(arg, frame);
            found = true;
        }
        else if (it->first.get()->load() == 0) {
            receive_pkt_hook pkt_hook = it->second;
            recv_hook hook = pkt_hook.hook;
            void* arg = pkt_hook.arg;
            hook(arg, frame);
            found = true;
        }
    }
    if (!found) {
        frames_mtx_.lock();
        frames_.push_back(frame);
        frames_mtx_.unlock();
    }
}
/* ----------- User packets not yet supported -----------
rtp_error_t uvgrtp::reception_flow::install_user_hook(void* arg, void (*hook)(void*, uint8_t* payload))
{
    if (!hook)
        return RTP_INVALID_VALUE;

    user_hook_ = hook;
    user_hook_arg_ = arg;

    return RTP_OK;
}

void uvgrtp::reception_flow::return_user_pkt(uint8_t* pkt)
{
    UVG_LOG_DEBUG("Received user packet");
    if (!pkt) {
        UVG_LOG_DEBUG("User packet empty");
        return;
    }
    if (user_hook_) {
        user_hook_(user_hook_arg_, pkt);
    }
    else {
        UVG_LOG_DEBUG("No user hook installed");
    }
}*/

void uvgrtp::reception_flow::receiver(std::shared_ptr<uvgrtp::socket> socket)
{
    int read_packets = 0;

    while (!should_stop_) {

        // First we wait using poll until there is data in the socket

#ifdef _WIN32
        LPWSAPOLLFD pfds = new pollfd();
#else
        pollfd* pfds = new pollfd();
#endif

        size_t read_fds = socket->get_raw_socket();
        pfds->fd = read_fds;
        pfds->events = POLLIN;

        // exits after this time if no data has been received to check whether we should exit
        int timeout_ms = 100; 

#ifdef _WIN32
        if (WSAPoll(pfds, 1, timeout_ms) < 0) {
#else
        if (poll(pfds, 1, timeout_ms) < 0) {
#endif
            UVG_LOG_ERROR("poll(2) failed");
            if (pfds)
            {
                delete pfds;
                pfds = nullptr;
            }
            break;
        }

        if (pfds->revents & POLLIN) {

            // we write as many packets as socket has in the buffer
            while (!should_stop_)
            {
                ssize_t next_write_index = next_buffer_location(last_ring_write_index_);

                //increase_buffer_size(next_write_index);

                rtp_error_t ret = RTP_OK;
                sockaddr_in sender = {};
                sockaddr_in6 sender6 = {};
                
                // get the potential packet
                ret = socket->recvfrom(ring_buffer_[next_write_index].data, payload_size_,
                    MSG_DONTWAIT, &sender, &sender6, &ring_buffer_[next_write_index].read);


                if (ret == RTP_INTERRUPTED)
                {
                    break;
                }
                else if (ring_buffer_[next_write_index].read == 0)
                {
                    UVG_LOG_WARN("Failed to read anything from socket");
                    break;
                }
                else if (ret != RTP_OK) {
                    UVG_LOG_ERROR("recvfrom(2) failed! Reception flow cannot continue %d!", ret);
                    should_stop_ = true;
                    break;
                }

                ++read_packets;
                // Save the IP adderss that this packet came from into the buffer
                ring_buffer_[next_write_index].from6 = sender6;
                ring_buffer_[next_write_index].from = sender;
                // finally we update the ring buffer so processing (reading) knows that there is a new frame
                last_ring_write_index_ = next_write_index;
            }

            // start processing the packets by waking the processing thread
            process_cond_.notify_one();
        }

        if (pfds)
        {
            delete pfds;
            pfds = nullptr;
        }
    }

    UVG_LOG_DEBUG("Total read packets from buffer: %li", read_packets);
}

void uvgrtp::reception_flow::process_packet(int rce_flags)
{
    std::unique_lock<std::mutex> lk(wait_mtx_);

    int processed_packets = 0;

    while (!should_stop_)
    {
        // go to sleep waiting for something to process
        process_cond_.wait(lk); 

        if (should_stop_)
        {
            break;
        }

        // process all available reads in one go
        while (ring_read_index_ != last_ring_write_index_)
        {
            // first update the read location
            ring_read_index_ = next_buffer_location(ring_read_index_);

            if (ring_buffer_[ring_read_index_].read > 0)
            {
                rtp_error_t ret = RTP_OK;
                /* When processing a packet, the following checks are done
                 * 1. Check the SSRC of the packets. This field is in the same place for RTP, RTCP and ZRTP. (+ SRTP/SRTCP)
                 * 2. If there is no SSRC match, this is a user packet.
                 * 3. Determine which protocol this packet belongs to. RTCP packets can be told apart from RTP packets via 
                 *    bits 8-15. ZRTP packets can be told apart from others via their 2 first its being 0 and the Magic Cookie
                 *    field being 0x5a525450. Holepunching is not needed if RTCP is enabled. If not, holepuncher packets
                 *    contain 0x00 payload.
                 * 4. After determining the correct protocol, hand out the packet to the correct handler if it exists.
                 */

                // zrtp headerit network byte orderissa, 32 bittiä pitkiä. rtp myös
                // process the ring buffer location through all the handlers

                for (auto& p : NEW_packet_handlers_) {

                    uvgrtp::frame::rtp_frame* frame = nullptr;

                    //sockaddr_in from = ring_buffer_[ring_read_index_].from;
                    //sockaddr_in6 from6 = ring_buffer_[ring_read_index_].from6;

                    uint8_t* ptr = (uint8_t*)ring_buffer_[ring_read_index_].data;

                    /* -------------------- SSRC checks -------------------- */
                    uint32_t packet_ssrc = ntohl(*(uint32_t*)&ptr[8]);
                    uint32_t current_ssrc = p.first.get()->load();
                    bool found = false;
                    if (current_ssrc == packet_ssrc) {
                        // Socket multiplexing, this handler is the correct one 
                        found = true;
                    }
                    else if (current_ssrc == 0) {
                        // No socket multiplexing
                        found = true;
                    }
                    if (!found) {
                        // No SSRC match found, skip this handler
                        // User pkt????
                        continue;
                    }
                    // Handler set is found
                    handler_new* handlers = &p.second;
                    /* -------------------- Protocol checks -------------------- */
                    /* Checks in the following order:
                     * 1. If RCE_RTCP_MUX && packet type is 200 - 204   -> RTCP packet    (or SRTCP)
                     * 2. Magic Cookie is 0x5a525450                    -> ZRTP packet
                     * 3. Version is 2                                  -> RTP packet     (or SRTP)
                     * 4. Version is 00                                 -> Keep-Alive/Holepuncher */
                    rtp_error_t retval;
                    size_t size = (size_t)ring_buffer_[ring_read_index_].read;

                     /* -------------------- RTCP check -------------------- */
                    if (rce_flags & RCE_RTCP_MUX) {
                        uint8_t pt = (uint8_t)ptr[1];
                        UVG_LOG_DEBUG("Received frame with pt %u", pt);
                        if (pt >= 200 && pt <= 204) {
                            retval = handlers->rtcp.handler(nullptr, rce_flags, &ptr[0], size, &frame);
                            break;
                        }
                    }

                    uint8_t version = (*(uint8_t*)&ptr[0] >> 6) & 0x3;

                    /* -------------------- ZRTP check --------------------------------- */
                    // Magic Cookie 0x5a525450
                    if (ntohl(*(uint32_t*)&ptr[4]) == 0x5a525450) {
                        if (handlers->zrtp.handler != nullptr) {
                            retval = handlers->zrtp.handler(nullptr, rce_flags, &ptr[0], size, &frame);
                        }
                        break;
                    }

                    /* -------------------- RTP check ---------------------------------- */
                    else if (version == 0x2) {
                        retval = RTP_PKT_MODIFIED;

                        /* Create RTP header */
                        if (handlers->rtp.handler != nullptr) {
                            retval = handlers->rtp.handler(nullptr, rce_flags, &ptr[0], size, &frame);
                        }
                        else {
                            /* This should only happen when ZRTP is enabled. If the remote stream is done first, they start sending
                             * media already before we have handled the last ZRTP ConfACK packet. This should not be a problem
                             * as we only lose the first frame or a few at worst. If this causes issues, the sender
                             * may, for example, sleep for 50 or so milliseconds to give us time to complete ZRTP. */
                            UVG_LOG_DEBUG("RTP handler is not (yet?) installed");
                        }

                        /* If SRTP is enabled -> send through SRTP handler */
                        if (rce_flags & RCE_SRTP && retval == RTP_PKT_MODIFIED) {
                            if (handlers->srtp.handler != nullptr) {
                                retval = handlers->srtp.handler(handlers->srtp.args, rce_flags, &ptr[0], size, &frame);
                            }
                        }
                        /* Update RTCP session statistics */ 
                        if (rce_flags & RCE_RTCP) {
                            if (handlers->rtcp_common.handler != nullptr) {
                                retval = handlers->rtcp_common.handler(handlers->rtcp_common.args, rce_flags, &ptr[0], size, &frame);
                            }
                        }
                        
                        /* If packet is ok, hand over to media handler */
                        if (retval == RTP_PKT_MODIFIED || retval == RTP_PKT_NOT_HANDLED) {
                            if (handlers->media.handler && frame) {
                                retval = handlers->media.handler(handlers->media.args, rce_flags, &ptr[0], size, &frame);
                            }
                            /* Last, if one or more packets are ready, return them to the user */
                            if (retval == RTP_PKT_READY) {
                                return_frame(frame);
                                break;
                            }
                            else if (retval == RTP_MULTIPLE_PKTS_READY && handlers->getter != nullptr) {
                                //UVG_LOG_INFO("TODO:is this correct???");
                                while (handlers->getter(&frame) == RTP_PKT_READY) {
                                    return_frame(frame);
                                }
                                break;
                            }
                        }
                        break;
                    }

                    /* -------------------- Holepuncher check -------------------------- */
                    else if (version == 0x00) {
                        /* In uvgRTP, holepuncher packets are packets with a payload of 0x00, as in RFC 6263 4.1
                         * This can be changed to other alternatives specified in the RFC if current 
                         * implementation causes problems with user packets. */ 
                        UVG_LOG_DEBUG("Holepuncher packet");
                        break;
                    }
               }

                // to make sure we don't process this packet again
                ring_buffer_[ring_read_index_].read = 0;
                ++processed_packets;
            }
            else
            {
#ifndef NDEBUG 
#ifndef __RTP_SILENT__
                ssize_t write = last_ring_write_index_;
                ssize_t read = ring_read_index_;
                UVG_LOG_DEBUG("Found invalid frame in read buffer: %li. R: %lli, W: %lli", 
                    ring_buffer_[ring_read_index_].read, read, write);
#endif
#endif
            }
        }
    }

    UVG_LOG_DEBUG("Total processed packets: %li", processed_packets);
}

ssize_t uvgrtp::reception_flow::next_buffer_location(ssize_t current_location)
{
/*
#ifndef NDEBUG
    if (current_location + 1 == ring_buffer_.size())
    {
        ssize_t read = ring_read_index_;
        ssize_t write = last_ring_write_index_;
        UVG_LOG_DEBUG("Ring buffer (%lli) rotation. R: %lli, W: %lli", ring_buffer_.size(), read, write);
    }
#endif // !NDEBUG
*/

    // rotates to beginning after buffer end
    return (current_location + 1) % ring_buffer_.size();
}

void uvgrtp::reception_flow::increase_buffer_size(ssize_t next_write_index)
{
    // create new buffer spaces if the process/read hasn't freed any spots on the ring buffer
    if (next_write_index == ring_read_index_)
    {
        // increase the buffer size by 25%
        ssize_t increase = ring_buffer_.size() / 4;
        if (increase == 0) // just so there is some increase
            ++increase;

        UVG_LOG_DEBUG("Reception buffer ran out, increasing the buffer size: %lli -> %lli",
            ring_buffer_.size(), ring_buffer_.size() + increase);
        for (unsigned int i = 0; i < increase; ++i)
        {
            ring_buffer_.insert(ring_buffer_.begin() + next_write_index, { new uint8_t[payload_size_] , -1 });
        }

        // this works, because we have just added increase amount of spaces
        ring_read_index_ += increase;
    }
}

int uvgrtp::reception_flow::clear_stream_from_flow(std::shared_ptr<std::atomic<std::uint32_t>> remote_ssrc)
{
    // Clear all the data structures
    if (hooks_.find(remote_ssrc) != hooks_.end()) {
        hooks_.erase(remote_ssrc);
    }
    if (NEW_packet_handlers_.find(remote_ssrc) != NEW_packet_handlers_.end()) {
        NEW_packet_handlers_.erase(remote_ssrc);
    }

    // If all the data structures are empty, return 1 which means that there is no streams left for this reception_flow
    // and it can be safely deleted
    if (hooks_.empty() && NEW_packet_handlers_.empty()) {
        return 1;
    }
    return 0;
}