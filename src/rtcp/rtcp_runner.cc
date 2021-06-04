#include "rtcp.hh"
#include "../poll.hh"

#include "debug.hh"

std::vector<uvgrtp::socket>& uvgrtp::rtcp::get_sockets()
{
    return sockets_;
}

std::vector<uint32_t> uvgrtp::rtcp::get_participants()
{
    std::vector<uint32_t> ssrcs;

    for (auto& i : participants_) {
        ssrcs.push_back(i.first);
    }

    return ssrcs;
}

rtp_error_t uvgrtp::rtcp::generate_report()
{
    rtcp_pkt_sent_count_++;

    if (our_role_ == RECEIVER)
        return generate_receiver_report();
    return generate_sender_report();
}

void uvgrtp::rtcp::rtcp_runner(uvgrtp::rtcp *rtcp)
{
    LOG_INFO("RTCP instance created!");

    uvgrtp::clock::hrc::hrc_t start, end;
    int nread, diff, timeout = MIN_TIMEOUT;
    uint8_t buffer[MAX_PACKET];
    rtp_error_t ret;

    while (rtcp->active()) {
        start = uvgrtp::clock::hrc::now();
        ret   = uvgrtp::poll::poll(rtcp->get_sockets(), buffer, MAX_PACKET, timeout, &nread);

        if (ret == RTP_OK && nread > 0) {
            (void)rtcp->handle_incoming_packet(buffer, (size_t)nread);
        } else if (ret == RTP_INTERRUPTED) {
            /* do nothing */
        } else {
            LOG_ERROR("recvfrom failed, %d", ret);
        }

        diff = (int)uvgrtp::clock::hrc::diff_now(start);

        if (diff >= MIN_TIMEOUT) {
            if ((ret = rtcp->generate_report()) != RTP_OK && ret != RTP_NOT_READY) {
                LOG_ERROR("Failed to send RTCP status report!");
            }

            timeout = MIN_TIMEOUT;
        }
    }
}
