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

constexpr size_t DEFAULT_INITIAL_BUFFER_SIZE = 4194304;

uvgrtp::socketfactory::socketfactory(int rce_flags) :
    frames_(),
    rce_flags_(rce_flags),
    local_address_(""),
    used_ports_({}),
    ipv6_(false),
    used_sockets_({}),
    recv_hook_arg_(nullptr),
    recv_hook_(nullptr),
    packet_handlers_({}),
    should_stop_(true),
    receiver_(nullptr),
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

    if (receiver_ != nullptr && receiver_->joinable())
    {
        receiver_->join();
    }

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
        used_sockets_.push_back(socket);
        return socket;
    }
    
    return nullptr;
}

rtp_error_t uvgrtp::socketfactory::bind_socket(std::shared_ptr<uvgrtp::socket> soc, uint16_t port)
{
    rtp_error_t ret = RTP_OK;
    if (std::find(used_ports_.begin(), used_ports_.end(), port) == used_ports_.end()) {

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
    void (*hook)(void*, uvgrtp::frame::rtp_frame*)
)
{
    if (!hook)
        return RTP_INVALID_VALUE;

    recv_hook_ = hook;
    recv_hook_arg_ = arg;

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

    //UVG_LOG_DEBUG("Creating receiving threads and setting priorities");
    //processor_ = std::unique_ptr<std::thread>(new std::thread(&uvgrtp::socketfactory::process_packet, this, rce_flags));
    receiver_ = std::unique_ptr<std::thread>(new std::thread(&uvgrtp::socketfactory::rcvr, this, socket));

    // set receiver thread priority to maximum
#ifndef WIN32
    struct sched_param params;
    params.sched_priority = sched_get_priority_max(SCHED_FIFO);
    pthread_setschedparam(receiver_->native_handle(), SCHED_FIFO, &params);
    //params.sched_priority = sched_get_priority_max(SCHED_FIFO) - 1;
    //pthread_setschedparam(processor_->native_handle(), SCHED_FIFO, &params);
#else

    SetThreadPriority(receiver_->native_handle(), REALTIME_PRIORITY_CLASS);
    //SetThreadPriority(processor_->native_handle(), ABOVE_NORMAL_PRIORITY_CLASS);

#endif

    return RTP_ERROR::RTP_OK;
}

std::shared_ptr<uvgrtp::socket> uvgrtp::socketfactory::get_socket_ptr() const
{
    return nullptr;
}

bool uvgrtp::socketfactory::get_ipv6() const
{
    return ipv6_;
}

void uvgrtp::socketfactory::rcvr(std::shared_ptr<uvgrtp::socket> socket)
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
            //UVG_LOG_ERROR("poll(2) failed");
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
                    //UVG_LOG_WARN("Failed to read anything from socket");
                    break;
                }
                else if (ret != RTP_OK) {
                    //UVG_LOG_ERROR("recvfrom(2) failed! Reception flow cannot continue %d!", ret);
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
        }

    UVG_LOG_DEBUG("Total read packets from buffer: %li", read_packets);
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