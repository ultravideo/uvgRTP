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
#include "rtp_opus.hh"
#include "rtp_hevc.hh"
#include "rtp_generic.hh"
#include "writer.hh"

kvz_rtp::writer::writer(std::string dst_addr, int dst_port):
    connection(false),
    dst_addr_(dst_addr),
    dst_port_(dst_port),
    src_port_(0),
    fqueue_()
{
}

kvz_rtp::writer::writer(std::string dst_addr, int dst_port, int src_port):
    writer(dst_addr, dst_port)
{
    src_port_ = src_port;
}

kvz_rtp::writer::~writer()
{
}

rtp_error_t kvz_rtp::writer::start()
{
    rtp_error_t ret;

    if ((ret = socket_.init(AF_INET, SOCK_DGRAM, 0)) != RTP_OK)
        return ret;

    /* if source port is not 0, writer should be bind to that port so that outgoing packet
     * has a correct source port (important for hole punching purposes) */
    if (src_port_ != 0) {
        int enable = 1;

        if ((ret = socket_.setsockopt(SOL_SOCKET, SO_REUSEADDR, (const char *)&enable, sizeof(int))) != RTP_OK)
            return ret;

        LOG_DEBUG("Binding to port %d (source port)", src_port_);

        if ((ret = socket_.bind(AF_INET, INADDR_ANY, src_port_)) != RTP_OK)
            return ret;
    }

    addr_out_ = socket_.create_sockaddr(AF_INET, dst_addr_, dst_port_);
    socket_.set_sockaddr(addr_out_);

    return RTP_OK;
}

rtp_error_t kvz_rtp::writer::push_frame(uint8_t *data, uint32_t data_len, rtp_format_t fmt, uint32_t timestamp)
{
    switch (fmt) {
        case RTP_FORMAT_HEVC:
            return kvz_rtp::hevc::push_frame(this, data, data_len, timestamp);

        case RTP_FORMAT_OPUS:
            return kvz_rtp::opus::push_frame(this, data, data_len, timestamp);

        default:
            LOG_DEBUG("Format not recognized, pushing the frame as generic");
            return kvz_rtp::generic::push_frame(this, data, data_len, timestamp);
    }
}

sockaddr_in kvz_rtp::writer::get_out_address()
{
    return addr_out_;
}

const kvz_rtp::frame_queue& kvz_rtp::writer::get_frame_queue() const
{
    return fqueue_;
}
