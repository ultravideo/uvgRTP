#include "socketfactory.hh"
#include "socket.hh"
#include "uvgrtp/frame.hh"
#include "random.hh"
#include "global.hh"
#include "debug.hh"


#ifdef _WIN32
#include <Ws2tcpip.h>
#define MSG_DONTWAIT 0
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <errno.h>
#include <poll.h>
#include <pthread.h>

#endif
#include <algorithm>
#include <cstring>
#include <iterator>

constexpr size_t DEFAULT_INITIAL_BUFFER_SIZE = 4194304;

uvgrtp::socketfactory::socketfactory(int rce_flags) :
    frames_(),
    rce_flags_(rce_flags),
    local_address_(""),
    used_ports_({}),
    ipv6_(false),
    used_sockets_({}),
    hooks_({}),
    packet_handlers_({}),
    should_stop_(true),
    receiver_(nullptr),
    receivers_(),
    processors_(),
    ring_buffer_(),
    ring_read_index_(-1), // invalid first index that will increase to a valid one
    last_ring_write_index_(-1),
    buffer_size_kbytes_(DEFAULT_INITIAL_BUFFER_SIZE),
    payload_size_(MAX_IPV4_PAYLOAD)
{
    create_ring_buffer();
}

uvgrtp::socketfactory::~socketfactory()
{
    destroy_ring_buffer();
    clear_frames();
}

void uvgrtp::socketfactory::clear_frames()
{
    frames_mtx_.lock();
    for (auto& frame : frames_)
    {
        (void)uvgrtp::frame::dealloc_frame(frame);
    }

    frames_.clear();
    frames_mtx_.unlock();
}

rtp_error_t uvgrtp::socketfactory::stop()
{
    should_stop_ = true;
    process_cond_.notify_all();
    
    for (auto r = receivers_.begin(); r != receivers_.end(); ++r) {
        if (*r != nullptr && r->get()->joinable())
        {
            r->get()->join();
        }
    }
    for (auto p = processors_.begin(); p != processors_.end(); ++p) {
        if (*p != nullptr && p->get()->joinable())
        {
            p->get()->join();
        }
    }
    /*
    if (receiver_ != nullptr && receiver_->joinable())
    {
        receiver_->join();
    }
    if (processor_ != nullptr && processor_->joinable())
    {
        processor_->join();
    }*/

    clear_frames();

    return RTP_OK;
}

rtp_error_t uvgrtp::socketfactory::set_local_interface(std::string local_addr)
{
    //rtp_error_t ret;

    local_address_ = local_addr;
    // check IP address family
    struct addrinfo hint, * res = NULL;
    memset(&hint, '\0', sizeof(hint));
    hint.ai_family = PF_UNSPEC;
    hint.ai_flags = AI_NUMERICHOST;

    if (getaddrinfo(local_address_.c_str(), NULL, &hint, &res) != 0) {
        return RTP_GENERIC_ERROR;
    }
    if (res->ai_family == AF_INET6) {
        ipv6_ = true;

    }
    return RTP_OK;
}

std::shared_ptr<uvgrtp::socket> uvgrtp::socketfactory::create_new_socket()
{
    rtp_error_t ret = RTP_OK;
    std::shared_ptr<uvgrtp::socket> socket = std::make_shared<uvgrtp::socket>(rce_flags_);

    if (ipv6_) {
        if ((ret = socket->init(AF_INET6, SOCK_DGRAM, 0)) != RTP_OK) {
            return nullptr;
        }
    }
    else {
        if ((ret = socket->init(AF_INET, SOCK_DGRAM, 0)) != RTP_OK) {
            return nullptr;
        }
    }
#ifdef _WIN32
    /* Make the socket non-blocking */
    int enabled = 1;

    if (::ioctlsocket(socket->get_raw_socket(), FIONBIO, (u_long*)&enabled) < 0) {
        return nullptr;
    }
#endif

    if (ret == RTP_OK) {
        socket_mutex_.lock();
        used_sockets_.push_back(socket);
        socket_mutex_.unlock();
        return socket;
    }
    
    return nullptr;
}

rtp_error_t uvgrtp::socketfactory::bind_socket(std::shared_ptr<uvgrtp::socket> soc, uint16_t port)
{
    rtp_error_t ret = RTP_OK;
    if (is_port_in_use(port) == false) {
        if (ipv6_) {
            sockaddr_in6 bind_addr6 = soc->create_ip6_sockaddr(local_address_, port);
            ret = soc->bind_ip6(bind_addr6);
        }
        else {
            sockaddr_in bind_addr = soc->create_sockaddr(AF_INET, local_address_, port);
            ret = soc->bind(bind_addr);
        }
        if (ret == RTP_OK) {
            used_ports_.push_back(port);
            return RTP_OK;
        }
    }
    return ret;
}

rtp_error_t uvgrtp::socketfactory::bind_socket_anyip(std::shared_ptr<uvgrtp::socket> soc, uint16_t port)
{
    rtp_error_t ret = RTP_OK;
    if (std::find(used_ports_.begin(), used_ports_.end(), port) == used_ports_.end()) {

        if (ipv6_) {
            sockaddr_in6 bind_addr6 = soc->create_ip6_sockaddr_any(port);
            ret = soc->bind_ip6(bind_addr6);
        }
        else {
            ret = soc->bind(AF_INET, INADDR_ANY, port);
        }
        if (ret == RTP_OK) {
            used_ports_.push_back(port);
            return RTP_OK;
        }
    }
    return ret;
}

rtp_error_t uvgrtp::socketfactory::install_receive_hook(
    void* arg,
    void (*hook)(void*, uvgrtp::frame::rtp_frame*),
    uint32_t ssrc
)
{
    if (!hook) {
        return RTP_INVALID_VALUE;
    }
    // t‰m‰ ssrc on sitten se meid‰n eli vastaanottajan p‰‰n media streamin ssrc
    if(hooks_.count(ssrc) == 0) {
        receive_pkt_hook new_hook = { arg, hook };
        hooks_[ssrc] = new_hook;
    }
    //recv_hook_ = hook;
    //recv_hook_arg_ = arg;

    return RTP_OK;
}

rtp_error_t uvgrtp::socketfactory::install_aux_handler(
    uint32_t key,
    void* arg,
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

rtp_error_t uvgrtp::socketfactory::install_aux_handler_cpp(uint32_t key,
    std::function<rtp_error_t(int, uvgrtp::frame::rtp_frame**)> handler,
    std::function<rtp_error_t(uvgrtp::frame::rtp_frame**)> getter)
{
    if (!handler)
        return RTP_INVALID_VALUE;

    if (packet_handlers_.find(key) == packet_handlers_.end())
        return RTP_INVALID_VALUE;

    auxiliary_handler_cpp ahc = { handler, getter };
    packet_handlers_[key].auxiliary_cpp.push_back(ahc);
    return RTP_OK;
}

uint32_t uvgrtp::socketfactory::install_handler(uvgrtp::packet_handler handler)
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

rtp_error_t uvgrtp::socketfactory::start(std::shared_ptr<uvgrtp::socket> socket, int rce_flags)
{
    should_stop_ = false;

    UVG_LOG_DEBUG("Creating receiving threads and setting priorities");
    std::unique_ptr<std::thread> receiver = std::unique_ptr<std::thread>(new std::thread(&uvgrtp::socketfactory::rcvr, this, socket));
    std::unique_ptr<std::thread> processor = std::unique_ptr<std::thread>(new std::thread(&uvgrtp::socketfactory::process_packet, this, rce_flags));

    // set receiver thread priority to maximum
#ifndef WIN32
    struct sched_param params;
    params.sched_priority = sched_get_priority_max(SCHED_FIFO);
    pthread_setschedparam(receiver->native_handle(), SCHED_FIFO, &params);
    params.sched_priority = sched_get_priority_max(SCHED_FIFO) - 1;
    pthread_setschedparam(processor->native_handle(), SCHED_FIFO, &params);
#else

    SetThreadPriority(receiver->native_handle(), REALTIME_PRIORITY_CLASS);
    SetThreadPriority(processor->native_handle(), ABOVE_NORMAL_PRIORITY_CLASS);

#endif
    receivers_.push_back(std::move(receiver));
    processors_.push_back(std::move(processor));
    return RTP_ERROR::RTP_OK;
}

std::shared_ptr<uvgrtp::socket> uvgrtp::socketfactory::get_socket_ptr() const
{
    if (!used_sockets_.empty()) {
        return used_sockets_.at(0);
    }
    return nullptr;
}

bool uvgrtp::socketfactory::get_ipv6() const
{
    return ipv6_;
}

bool uvgrtp::socketfactory::is_port_in_use(uint16_t port) const
{
    if (std::find(used_ports_.begin(), used_ports_.end(), port) == used_ports_.end()) {
        return false;
    }
    return true;
}

void uvgrtp::socketfactory::rcvr(std::shared_ptr<uvgrtp::socket> socket)
{
    //int n = 0;

    int read_packets = 0;
    while (!should_stop_) {
        //socket_mutex_.lock();
        //for (auto s = used_sockets_.begin(); s != used_sockets_.end(); ++s) {
            //std::shared_ptr<uvgrtp::socket> socket = *s;
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

                    // get the potential packet
                    ret = socket->recvfrom(ring_buffer_[next_write_index].data, payload_size_,
                        MSG_DONTWAIT, &ring_buffer_[next_write_index].read);

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
           // ++n;
       // }
       // UVG_LOG_DEBUG("Packets from socket read");
        //socket_mutex_.unlock();
    }
    UVG_LOG_DEBUG("Total read packets from buffer: %li", read_packets);
}

void uvgrtp::socketfactory::process_packet(int rce_flags)
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

void uvgrtp::socketfactory::call_aux_handlers(uint32_t key, int rce_flags, uvgrtp::frame::rtp_frame** frame)
{
    rtp_error_t ret;
    for (auto& aux : packet_handlers_[key].auxiliary) {
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

uvgrtp::frame::rtp_frame* uvgrtp::socketfactory::pull_frame()
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

uvgrtp::frame::rtp_frame* uvgrtp::socketfactory::pull_frame(ssize_t timeout_ms)
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

ssize_t uvgrtp::socketfactory::next_buffer_location(ssize_t current_location)
{
    // rotates to beginning after buffer end
    return (current_location + 1) % ring_buffer_.size();
}

void uvgrtp::socketfactory::create_ring_buffer()
{
    destroy_ring_buffer();
    size_t elements = buffer_size_kbytes_ / payload_size_;

    for (size_t i = 0; i < elements; ++i)
    {
        uint8_t* data = new uint8_t[payload_size_];
        if (data)
        {
            ring_buffer_.push_back({ data, 0 });
        }
        else
        {
            UVG_LOG_ERROR("Failed to allocate memory for ring buffer");
        }
    }
}

void uvgrtp::socketfactory::destroy_ring_buffer()
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

void uvgrtp::socketfactory::return_frame(uvgrtp::frame::rtp_frame* frame)
{
    uint32_t ssrc = frame->header.ssrc;
    if(hooks_.count(ssrc) > 0) {

        receive_pkt_hook pkt_hook = hooks_[ssrc];
        recv_hook hook = pkt_hook.hook;
        void* arg = pkt_hook.arg;
        hook(arg, frame);
    }
    else {
        frames_mtx_.lock();
        frames_.push_back(frame);
        frames_mtx_.unlock();
    }
}