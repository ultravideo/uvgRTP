#include "rtcp_reader.hh"
#include "uvgrtp/util.hh"
#include "uvgrtp/frame.hh"
#include "rtcp_packets.hh"
#include "poll.hh"
#include "uvgrtp/rtcp.hh"

#include "socket.hh"
#include "global.hh"
#include "debug.hh"

#ifndef _WIN32
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#else
#include <ws2ipdef.h>
#endif

const int MAX_PACKET = 65536;

uvgrtp::rtcp_reader::rtcp_reader(std::shared_ptr<uvgrtp::socketfactory> sfp) :
    active_(false),
    sfp_(sfp),
    socket_(nullptr),
    rtcps_map_({})
{

}

uvgrtp::rtcp_reader::~rtcp_reader()
{
}

void uvgrtp::rtcp_reader::rtcp_report_reader() {

    UVG_LOG_INFO("RTCP report reader created!");
    std::unique_ptr<uint8_t[]> buffer = std::unique_ptr<uint8_t[]>(new uint8_t[MAX_PACKET]);

    rtp_error_t ret = RTP_OK;
    int max_poll_timeout_ms = 100;


    while (active_) {
        int nread = 0;

        std::vector<std::shared_ptr<uvgrtp::socket>> temp = {};
        temp.push_back(socket_);

        ret = uvgrtp::poll::poll(temp, buffer.get(), MAX_PACKET, max_poll_timeout_ms, &nread);

        if (ret == RTP_OK && nread > 0)
        {
            uint32_t sender_ssrc = ntohl(*(uint32_t*)&buffer.get()[0 + RTCP_HEADER_SIZE]);
            for (auto& p : rtcps_map_) {
                if (sender_ssrc == p.first) {
                    std::shared_ptr<uvgrtp::rtcp> rtcp_ptr = p.second;
                    (void)rtcp_ptr->handle_incoming_packet(buffer.get(), (size_t)nread);
                }
           
            }
        }
        else if (ret == RTP_INTERRUPTED) {
            /* do nothing */
        }
        else {
            UVG_LOG_ERROR("poll failed, %d", ret);
            break; // TODO the sockets should be manages so that this is not needed
        }
    }
    UVG_LOG_DEBUG("Exited RTCP report reader loop");
}

bool uvgrtp::rtcp_reader::set_socket(std::shared_ptr<uvgrtp::socket> socket)
{
    socket_ = socket;
    return true;
}