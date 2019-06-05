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
    src_port_(0)
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
#ifdef _WIN32
    if ((socket_ = socket(AF_INET, SOCK_DGRAM, 0)) == INVALID_SOCKET) {
#else
    if ((socket_ = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
#endif
        LOG_ERROR("Creating socket failed: %s", strerror(errno));
        return RTP_SOCKET_ERROR;
    }

    /* if source port is not 0, writer should be bind to that port so that outgoing packet
     * has a correct source port (important for hole punching purposes) */
    if (src_port_ != 0) {
        int enable = 1;

        if (setsockopt(socket_, SOL_SOCKET, SO_REUSEADDR, (const char *)&enable, sizeof(int)) < 0) {
            LOG_ERROR("Failed to set socket options: %s", strerror(errno));
            return RTP_GENERIC_ERROR;
        }

        LOG_DEBUG("Binding to port %d (source port)", src_port_);

        sockaddr_in addr_in;

        memset(&addr_in, 0, sizeof(addr_in));
        addr_in.sin_family = AF_INET;
        addr_in.sin_addr.s_addr = htonl(INADDR_ANY);
        addr_in.sin_port = htons(src_port_);

        if (bind(socket_, (struct sockaddr *) &addr_in, sizeof(addr_in)) < 0) {
            LOG_ERROR("Binding failed: %s", strerror(errno));
            return RTP_BIND_ERROR;
        }
    }

    memset(&addr_out_, 0, sizeof(addr_out_));
    addr_out_.sin_family = AF_INET;

    inet_pton(AF_INET, dst_addr_.c_str(), &addr_out_.sin_addr);
    addr_out_.sin_port = htons((uint16_t)dst_port_);

    return RTP_OK;
}

rtp_error_t kvz_rtp::writer::push_frame(uint8_t *data, uint32_t data_len, rtp_format_t fmt, uint32_t timestamp)
{
    switch (fmt) {
        case RTP_FORMAT_HEVC:
            return kvz_rtp::hevc::push_hevc_frame(this, data, data_len, timestamp);

        case RTP_FORMAT_OPUS:
            return kvz_rtp::opus::push_opus_frame(this, data, data_len, timestamp);

        default:
            LOG_DEBUG("Format not recognized, pushing the frame as generic");
            return kvz_rtp::generic::push_generic_frame(this, data, data_len, timestamp);
    }
}

sockaddr_in kvz_rtp::writer::get_out_address()
{
    return addr_out_;
}
