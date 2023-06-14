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
    hooks_({}),
    handler_mapping_({}),
    should_stop_(true),
    receiver_(nullptr),
    //user_hook_arg_(nullptr),
    //user_hook_(nullptr),
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
            ring_buffer_.push_back({data, 0});
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

uint32_t uvgrtp::reception_flow::install_handler(uvgrtp::packet_handler handler)
{
    uint32_t key;

    if (!handler)
        return 0;

    do {
        key = uvgrtp::random::generate_32();
    } while (!key || (packet_handlers_.find(key) != packet_handlers_.end()));

    packet_handlers_[key].primary = handler;
    return key;
}

rtp_error_t uvgrtp::reception_flow::install_aux_handler(
    uint32_t key,
    void *arg,
    uvgrtp::packet_handler_aux handler,
    uvgrtp::frame_getter getter
)
{
    if (!handler)
        return RTP_INVALID_VALUE;

    if (packet_handlers_.find(key) == packet_handlers_.end())
        return RTP_INVALID_VALUE;

    auxiliary_handler aux;
    aux.arg = arg;
    aux.getter = getter;
    aux.handler = handler;

    packet_handlers_[key].auxiliary.push_back(aux);
    return RTP_OK;
}

rtp_error_t uvgrtp::reception_flow::install_aux_handler_cpp(uint32_t key,
    std::function<rtp_error_t(int, uvgrtp::frame::rtp_frame**)> handler,
    std::function<rtp_error_t(uvgrtp::frame::rtp_frame**)> getter)
{
    if (!handler)
        return RTP_INVALID_VALUE;

    if (packet_handlers_.find(key) == packet_handlers_.end())
        return RTP_INVALID_VALUE;

    auxiliary_handler_cpp ahc = {handler, getter};
    packet_handlers_[key].auxiliary_cpp.push_back(ahc);
    return RTP_OK;
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

void uvgrtp::reception_flow::call_aux_handlers(uint32_t key, int rce_flags, uvgrtp::frame::rtp_frame **frame)
{
    rtp_error_t ret;

    for (auto& aux : packet_handlers_[key].auxiliary) {

        auto fr = *frame;
        uint32_t pkt_ssrc = fr->header.ssrc;
        uint32_t current_ssrc = handler_mapping_[key].get()->load();
        bool found = false;
        if (current_ssrc == pkt_ssrc) {
            found = true;
        }
        else if (current_ssrc == 0) {
            found = true;
        }
        if (!found) {
            // No SSRC match found, skip this handler
            continue;
        }

        switch ((ret = (*aux.handler)(aux.arg, rce_flags, frame))) {
            /* packet was handled successfully */
            case RTP_OK:
                break;

            case RTP_MULTIPLE_PKTS_READY:
            {
                while ((*aux.getter)(aux.arg, frame) == RTP_PKT_READY)
                    return_frame(*frame);
            }
            break;

            case RTP_PKT_READY:
                return_frame(*frame);
                break;

            /* packet was not handled or only partially handled by the handler
             * proceed to the next handler */
            case RTP_PKT_NOT_HANDLED:
            case RTP_PKT_MODIFIED:
                continue;

            case RTP_GENERIC_ERROR:
                // too many prints with this in case of minor errors
                //UVG_LOG_DEBUG("Error in auxiliary handling of received packet!");
                break;

            default:
                UVG_LOG_ERROR("Unknown error code from packet handler: %d", ret);
                break;
        }
    }

    for (auto& aux : packet_handlers_[key].auxiliary_cpp) {
        switch ((ret = aux.handler(rce_flags, frame))) {
            
        case RTP_OK: /* packet was handled successfully */
        {
            break;
        }
        case RTP_MULTIPLE_PKTS_READY:
        {
            while (aux.getter(frame) == RTP_PKT_READY)
                return_frame(*frame);

            break;
        }
        case RTP_PKT_READY:
        {
            return_frame(*frame);
            break;
        }

            /* packet was not handled or only partially handled by the handler
             * proceed to the next handler */
        case RTP_PKT_NOT_HANDLED:
        {
            continue;
        }
        case RTP_PKT_MODIFIED:
        {
            continue;
        }

        case RTP_GENERIC_ERROR:
        {
            // too many prints with this in case of minor errors
            //UVG_LOG_DEBUG("Error in auxiliary handling of received packet (cpp)!");
            break;
        }

        default:
        {
            UVG_LOG_ERROR("Unknown error code from packet handler: %d", ret);
            break;
        }
        }
    }
}

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

                // process the ring buffer location through all the handlers
                for (auto& handler : packet_handlers_) {
                    uvgrtp::frame::rtp_frame* frame = nullptr;
                    frame = (uvgrtp::frame::rtp_frame*)ring_buffer_[ring_read_index_].data;
                    sockaddr_in from = ring_buffer_[ring_read_index_].from;
                    sockaddr_in6 from6 = ring_buffer_[ring_read_index_].from6;

                    uint8_t* ptr = (uint8_t*)ring_buffer_[ring_read_index_].data;
                    uint32_t nhssrc = ntohl(*(uint32_t*)&ptr[8]);
                    uint32_t hnssrc = (uint32_t)ptr[8];

                    uint32_t current_ssrc = handler_mapping_[handler.first].get()->load();
                    bool found = false;
                    // this looks so weird because the ssrc field in RTP packets is in different byte order 
                    // than in SRTP packets, so we have to check many different possibilities
                    // TODO: fix the byte order...
                    if (current_ssrc == hnssrc || current_ssrc == nhssrc|| current_ssrc == frame->header.ssrc) {
                        found = true;
                    }
                    else if (current_ssrc == 0) {
                        found = true;
                    }
                    if (!found) {
                        // No SSRC match found, skip this handler
                        continue;
                    }

                    frame = nullptr;
                    // rtcp packet types 200 201 202 203 204
                    uint8_t pt = (uint8_t)&ptr[1];
                    UVG_LOG_DEBUG("Received frame with pt %u", pt);
                    if (rce_flags & RCE_RTCP_MULTIPLEX) {
                        if ((uint8_t)&ptr[1] >= 200 && (uint8_t)&ptr[1] <= 204) {
                            rtcp_map_mutex_.lock();
                            for (auto& p : rtcps_map_) {
                                std::shared_ptr<uvgrtp::rtcp> rtcp_ptr = p.second;
                                if (current_ssrc == p.first.get()->load()) {
                                    (void)rtcp_ptr->handle_incoming_packet(ring_buffer_[ring_read_index_].data, (size_t)ring_buffer_[ring_read_index_].read);
                                }
                                else if (p.first.get()->load() == 0) {
                                    (void)rtcp_ptr->handle_incoming_packet(ring_buffer_[ring_read_index_].data, (size_t)ring_buffer_[ring_read_index_].read);
                                }
                            }
                            rtcp_map_mutex_.unlock();
                        }
                    }

                    // Here we don't lock ring mutex because the chaging is only done above. 
                    // NOTE: If there is a need for multiple processing threads, the read should be guarded
                    switch ((ret = (*handler.second.primary)(ring_buffer_[ring_read_index_].read,
                        ring_buffer_[ring_read_index_].data, rce_flags, &frame))) {
                        case RTP_OK:
                        {
                            // packet was handled successfully
                            break;
                        }
                        case RTP_PKT_NOT_HANDLED:
                        {
                            /* ----------- User packets not yet supported -----------
                            std::string from_str;
                            if (ipv6_) {
                                from_str = uvgrtp::socket::sockaddr_ip6_to_string(from6);
                            }
                            else {
                                from_str = uvgrtp::socket::sockaddr_to_string(from);
                            }
                            UVG_LOG_DEBUG("User packet from ip: %s", from_str.c_str());
                            return_user_pkt(ptr);
                            */

                            // packet was not handled by this primary handlers, proceed to the next one
                            continue;
                            /* packet was handled by the primary handler
                             * and should be dispatched to the auxiliary handler(s) */
                        }
                        case RTP_PKT_MODIFIED:
                        {
                            call_aux_handlers(handler.first, rce_flags, &frame);
                            break;
                        }
                        case RTP_GENERIC_ERROR:
                        {
                            UVG_LOG_DEBUG("Error in handling of received packet!");
                            break;
                        }
                        default:
                        {
                            UVG_LOG_ERROR("Unknown error code from packet handler: %d", ret);
                            break;
                        }
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

bool uvgrtp::reception_flow::map_handler_key(uint32_t key, std::shared_ptr<std::atomic<std::uint32_t>> remote_ssrc)
{
    if (handler_mapping_.find(key) == handler_mapping_.end()) {
        handler_mapping_[key] = remote_ssrc;
        return true;
    }
    return false;
}

rtp_error_t uvgrtp::reception_flow::map_rtcp_to_rec(std::shared_ptr<std::atomic<uint32_t>> ssrc, std::shared_ptr<uvgrtp::rtcp> rtcp)
{
    rtp_error_t ret = RTP_GENERIC_ERROR;
    rtcp_map_mutex_.lock();
    if (rtcps_map_.find(ssrc) == rtcps_map_.end()) {
        rtcps_map_[ssrc] = rtcp;
        ret = RTP_OK;
    }
    rtcp_map_mutex_.unlock();
    return ret;
}

int uvgrtp::reception_flow::clear_stream_from_flow(std::shared_ptr<std::atomic<std::uint32_t>> remote_ssrc, uint32_t handler_key)
{
    // Clear all the data structures
    if (hooks_.find(remote_ssrc) != hooks_.end()) {
        hooks_.erase(remote_ssrc);
    }
    if (packet_handlers_.find(handler_key) != packet_handlers_.end()) {
        packet_handlers_.erase(handler_key);
    }
    if (handler_mapping_.find(handler_key) != handler_mapping_.end()) {
        handler_mapping_.erase(handler_key);
    }
    // If all the data structures are empty, return 1 which means that there is no streams left for this reception_flow
    // and it can be safely deleted
    if (hooks_.empty() && packet_handlers_.empty() && handler_mapping_.empty()) {
        return 1;
    }
    return 0;
}

rtp_error_t uvgrtp::reception_flow::clear_rtcp_from_rec(std::shared_ptr<std::atomic<std::uint32_t>> remote_ssrc)
{
    rtp_error_t ret = RTP_GENERIC_ERROR;
    rtcp_map_mutex_.lock();
    if (rtcps_map_.find(remote_ssrc) != rtcps_map_.end()) {
        rtcps_map_.erase(remote_ssrc);
        ret = RTP_OK;
    }
    rtcp_map_mutex_.unlock();
    return ret;
}