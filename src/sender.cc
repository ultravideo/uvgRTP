#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#endif

#if defined(__MINGW32__) || defined(__MINGW64__)
#include "mingw_inet.hh"
using namespace kvz_rtp;
using namespace mingw;
#endif

#include <cstring>
#include <iostream>

#include "debug.hh"
#include "dispatch.hh"
#include "sender.hh"

#include "formats/opus.hh"
#include "formats/hevc.hh"
#include "formats/generic.hh"

kvz_rtp::sender::sender(kvz_rtp::socket& socket, rtp_ctx_conf& conf, rtp_format_t fmt, kvz_rtp::rtp *rtp):
    socket_(socket),
    rtp_(rtp),
    conf_(conf),
    fmt_(fmt)
{
}

kvz_rtp::sender::~sender()
{
    delete dispatcher_;
    delete fqueue_;
}

rtp_error_t kvz_rtp::sender::destroy()
{
    if (fmt_ == RTP_FORMAT_HEVC && conf_.flags & RCE_SYSTEM_CALL_DISPATCHER) {
        while (dispatcher_->stop() != RTP_OK) {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
    }

    return RTP_OK;
}

rtp_error_t kvz_rtp::sender::init()
{
    rtp_error_t ret  = RTP_OK;
    ssize_t buf_size = conf_.ctx_values[RCC_UDP_BUF_SIZE];

    if (buf_size <= 0)
        buf_size = 4 * 1000 * 1000;

    if ((ret = socket_.setsockopt(SOL_SOCKET, SO_SNDBUF, (const char *)&buf_size, sizeof(int))) != RTP_OK)
        return ret;

#ifndef _WIN32
    if (fmt_ == RTP_FORMAT_HEVC && conf_.flags & RCE_SYSTEM_CALL_DISPATCHER) {
        dispatcher_ = new kvz_rtp::dispatcher(&socket_);
        fqueue_     = new kvz_rtp::frame_queue(fmt_, conf_, dispatcher_);

        if (dispatcher_)
            dispatcher_->start();
    } else {
#endif
        fqueue_     = new kvz_rtp::frame_queue(fmt_, conf_);
        dispatcher_ = nullptr;
#ifndef _WIN32
    }
#endif

    return ret;
}

rtp_error_t kvz_rtp::sender::push_frame(uint8_t *data, size_t data_len, int flags)
{
    if (flags & RTP_COPY) {
        std::unique_ptr<uint8_t[]> data_ptr = std::unique_ptr<uint8_t[]>(new uint8_t[data_len]);
        std::memcpy(data_ptr.get(), data, data_len);

        return push_frame(std::move(data_ptr), data_len, 0);
    }

    switch (fmt_) {
        case RTP_FORMAT_HEVC:
            return kvz_rtp::hevc::push_frame(this, data, data_len, flags);

        case RTP_FORMAT_OPUS:
            return kvz_rtp::opus::push_frame(this, data, data_len, flags);

        default:
            LOG_DEBUG("Format not recognized, pushing the frame as generic");
            return kvz_rtp::generic::push_frame(this, data, data_len, flags);
    }
}

rtp_error_t kvz_rtp::sender::push_frame(std::unique_ptr<uint8_t[]> data, size_t data_len, int flags)
{
    switch (fmt_) {
        case RTP_FORMAT_HEVC:
            return kvz_rtp::hevc::push_frame(this, std::move(data), data_len, flags);

        case RTP_FORMAT_OPUS:
            return kvz_rtp::opus::push_frame(this, std::move(data), data_len, flags);

        default:
            LOG_DEBUG("Format not recognized, pushing the frame as generic");
            return kvz_rtp::generic::push_frame(this, std::move(data), data_len, flags);
    }
}

kvz_rtp::frame_queue *kvz_rtp::sender::get_frame_queue()
{
    return fqueue_;
}

kvz_rtp::socket& kvz_rtp::sender::get_socket()
{
    return socket_;
}

kvz_rtp::rtp *kvz_rtp::sender::get_rtp_ctx()
{
    return rtp_;
}

void kvz_rtp::sender::install_dealloc_hook(void (*dealloc_hook)(void *))
{
    if (!fqueue_)
        return;

    fqueue_->install_dealloc_hook(dealloc_hook);
}
