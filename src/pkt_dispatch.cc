#ifdef __linux__
#include <errno.h>

#else
#define MSG_DONTWAIT 0
#endif

#include <cstring>

#include "debug.hh"
#include "pkt_dispatch.hh"
#include "util.hh"

uvg_rtp::pkt_dispatcher::pkt_dispatcher(uvg_rtp::socket socket):
    socket_(socket)
{
}

uvg_rtp::pkt_dispatcher::~pkt_dispatcher()
{
}

rtp_error_t uvg_rtp::pkt_dispatcher::install_handler(uvg_rtp::packet_handler handler)
{
    if (!handler)
        return RTP_INVALID_VALUE;

    packet_handlers_.push_back(handler);
    return RTP_OK;
}

std::vector<uvg_rtp::packet_handler>& uvg_rtp::pkt_dispatcher::get_handlers()
{
    return packet_handlers_;
}

/* The point of packet dispatcher is to provide much-needed isolation between different layers
 * of uvgRTP. For example, HEVC handler should not concern itself with RTP packet validation
 * because that should be a global operation done for all packets.
 *
 * Neither should Opus handler take SRTP-provided authentication tag into account when it is
 * performing operations on the packet.
 * And ZRTP packetshould not be relayed from media handler to ZRTP handler et cetera.
 *
 * This can be achieved by having a global UDP packet handler for any packet type that validates
 * all common stuff it can and then dispatches the validated packet to the correct layer using
 * one of the installed handlers.
 *
 * If it's unclear as to which handler should be called, the packet is dispatcher to all relevant
 * handlers and a handler then returns RTP_OK/RTP_PKT_NOT_HANDLED based on whether the packet was handled.
 *
 * For example, if runner detects an incoming ZRTP packet, that packet is immediately dispatched to the
 * installed ZRTP handler if ZRTP has been enabled.
 * Likewise, if RTP packet authentication has been enabled, runner validates the packet before passing
 * it onto any other layer so all future work on the packet is not done in vain due to invalid data
 *
 * One piece of design choice that complicates the design of packet dispatcher a little is that the order
 * of handlers is important. First handler must be ZRTP and then follows SRTP, RTP and finally media handlers.
 * This requirement gives packet handler a clean and generic interface while giving a possibility to modify
 * the packet in each of the called handlers if needed. For example SRTP handler verifies RTP authentication
 * tag and decrypts the packet and RTP handler verifies the fields of the RTP packet and processes it into
 * a more easily modifiable format for the media handler. */
static void runner(uvg_rtp::pkt_dispatcher *dispatcher, uvg_rtp::socket& socket)
{
    int nread;
    fd_set read_fds;
    rtp_error_t ret;
    struct timeval t_val;

    FD_ZERO(&read_fds);

    t_val.tv_sec  = 0;
    t_val.tv_usec = 1500;

    const size_t recv_buffer_len = 8192;
    uint8_t recv_buffer[recv_buffer_len] = { 0 };

    while (dispatcher->active()) {
        FD_SET(socket.get_raw_socket(), &read_fds);
        int sret = ::select(socket.get_raw_socket() + 1, &read_fds, nullptr, nullptr, &t_val);

        if (sret < 0) {
            log_platform_error("select(2) failed");
            break;
        }

        do {
            if ((ret = socket.recvfrom(recv_buffer, recv_buffer_len, MSG_DONTWAIT, &nread)) == RTP_INTERRUPTED)
                break;

            if (ret != RTP_OK) {
                LOG_ERROR("recvfrom(2) failed! Packet dispatcher cannot continue %d!", ret);
                break;
            }

            for (auto& handler : dispatcher->get_handlers()) {
                switch ((ret = (*handler)(nread, recv_buffer))) {
                    /* packet was handled successfully or the packet was in some way corrupted */
                    case RTP_OK:
                        break;

                    /* the received packet is not handled at all or only partially by the called handler
                     * proceed to the next handler */
                    case RTP_PKT_NOT_HANDLED:
                    case RTP_PKT_MODIFIED:
                        continue;

                    case RTP_GENERIC_ERROR:
                        LOG_DEBUG("Received a corrputed packet!");
                        break;

                    default:
                        LOG_ERROR("Unknown error code from packet handler: %d", ret);
                        break;
                }

            }
        } while (ret == RTP_OK);
    }
}
