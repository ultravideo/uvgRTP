#include <cstring>
#include <iostream>

#include "debug.hh"
#include "frame.hh"
#include "reader.hh"

kvz_rtp::reader::reader(std::string src_addr, int src_port):
    connection(true),
    src_addr_(src_addr),
    src_port_(src_port),
    active_(false),
    recv_hook_(nullptr),
    recv_hook_arg_(nullptr)
{
}

kvz_rtp::reader::~reader()
{
    active_ = false;
    delete recv_buffer_;

    if (!framesOut_.empty()) {
        for (auto &i : framesOut_) {
            if (kvz_rtp::frame::dealloc_frame(i) != RTP_OK) {
                LOG_ERROR("Failed to deallocate frame!");
            }
        }
    }
}

rtp_error_t kvz_rtp::reader::start()
{
    LOG_INFO("Starting to listen to port %d", src_port_);

    if ((socket_ = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        LOG_ERROR("Failed to create socket: %s", strerror(errno));
        return RTP_SOCKET_ERROR;
    }

    int enable = 1;
    if (setsockopt(socket_, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0)
        perror("setsockopt(SO_REUSEADDR) failed");

    sockaddr_in addrIn_;

    memset(&addrIn_, 0, sizeof(addrIn_));
    addrIn_.sin_family = AF_INET;  
    addrIn_.sin_addr.s_addr = htonl(INADDR_ANY);
    addrIn_.sin_port = htons(src_port_);

    if (bind(socket_, (struct sockaddr *) &addrIn_, sizeof(addrIn_)) < 0) {
        LOG_ERROR("Failed to bind to port: %s", strerror(errno));
        return RTP_BIND_ERROR;
    }

    recv_buffer_len_ = MAX_PACKET;

    if ((recv_buffer_ = new uint8_t[MAX_PACKET]) == nullptr) {
        LOG_ERROR("Failed to allocate buffer for incoming data!");
        recv_buffer_len_ = 0;
    }

    active_ = true;

    runner_ = new std::thread(frame_receiver, this);
    runner_->detach();

    return RTP_OK;
}

kvz_rtp::frame::rtp_frame *kvz_rtp::reader::pull_frame()
{
    while (framesOut_.empty()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    frames_mtx_.lock();
    auto nextFrame = framesOut_.front();
    framesOut_.erase(framesOut_.begin());
    frames_mtx_.unlock();

    return nextFrame;
}

bool kvz_rtp::reader::active()
{
    return active_;
}

uint8_t *kvz_rtp::reader::get_recv_buffer() const
{
    return recv_buffer_;
}

uint32_t kvz_rtp::reader::get_recv_buffer_len() const
{
    return recv_buffer_len_;
}

void kvz_rtp::reader::add_outgoing_frame(kvz_rtp::frame::rtp_frame *frame)
{
    if (!frame)
        return;

    framesOut_.push_back(frame);
}

bool kvz_rtp::reader::recv_hook_installed()
{
    return recv_hook_ != nullptr;
}

void kvz_rtp::reader::install_recv_hook(void *arg, void (*hook)(void *arg, kvz_rtp::frame::rtp_frame *))
{
    if (hook == nullptr) {
        LOG_ERROR("Unable to install receive hook, function pointer is nullptr!");
        return;
    }

    recv_hook_     = hook;
    recv_hook_arg_ = arg;
}

void kvz_rtp::reader::recv_hook(kvz_rtp::frame::rtp_frame *frame)
{
    if (recv_hook_)
        return recv_hook_(recv_hook_arg_, frame);
}

int kvz_rtp::reader::frame_receiver(kvz_rtp::reader *reader)
{
    LOG_INFO("frameReceiver starting listening...");

    sockaddr_in from_addr;

    while (reader->active()) {
        int from_addrSize = sizeof(from_addr);
        int32_t ret = recvfrom(reader->get_socket(), reader->get_recv_buffer(), reader->get_recv_buffer_len(),
                               0, /* no flags */
#ifdef _WIN32
                               (SOCKADDR *)&from_addr, 
                               &from_addrSize
#else
                               (struct sockaddr *)&from_addr,
                               (socklen_t *)&from_addrSize
#endif
                );

        if (ret == -1) {
#if _WIN32
            int _error = WSAGetLastError();
            if (_error != 10035)
                std::cerr << "Socket error" << _error << std::endl;
#else
            perror("frameReceiver");
#endif
            return -1;
        } else {
            LOG_DEBUG("got %d bytes", ret);

            uint8_t *inbuf = reader->get_recv_buffer();
            auto *frame    = kvz_rtp::frame::alloc_frame(ret, kvz_rtp::frame::FRAME_TYPE_GENERIC);

            if (!frame) {
                LOG_ERROR("Failed to allocate RTP Frame!");
                continue;
            }

            frame->marker    = (inbuf[1] & 0x80) ? 1 : 0;
            frame->payload   = (inbuf[1] & 0x7f);
            frame->seq       = ntohs(*(uint16_t *)&inbuf[2]);
            frame->timestamp = ntohl(*(uint32_t *)&inbuf[4]);
            frame->ssrc      = ntohl(*(uint32_t *)&inbuf[8]);

            if (ret - 12 <= 0) {
                LOG_WARN("Got an invalid payload of size %d", ret);
                continue;
            }

            frame->header     = new uint8_t[ret];
            frame->header_len = ret;

            if (!frame->data) {
                LOG_ERROR("Failed to allocate buffer for RTP frame!");
                continue;
            }

            memcpy(frame->header, inbuf, ret);

            if (reader->recv_hook_installed())
                reader->recv_hook(frame);
            else
                reader->add_outgoing_frame(frame);
        }
    }
    LOG_INFO("FrameReceiver thread exiting...");
}
