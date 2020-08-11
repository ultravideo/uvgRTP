#ifdef _WIN32
#else
#endif

#include "rtcp.hh"
#include "poll.hh"

std::vector<uvg_rtp::socket>& uvg_rtp::rtcp::get_sockets()
{
    return sockets_;
}

std::vector<uint32_t> uvg_rtp::rtcp::get_participants()
{
    std::vector<uint32_t> ssrcs;

    for (auto& i : participants_) {
        ssrcs.push_back(i.first);
    }

    return ssrcs;
}

rtp_error_t uvg_rtp::rtcp::generate_report()
{
    if (our_role_ == RECEIVER)
        return generate_receiver_report();
    return generate_sender_report();
}

void uvg_rtp::rtcp::rtcp_runner(uvg_rtp::rtcp *rtcp)
{
    LOG_INFO("RTCP instance created!");

    uvg_rtp::clock::hrc::hrc_t start, end;
    int nread, diff, timeout = MIN_TIMEOUT;
    uint8_t buffer[MAX_PACKET];
    rtp_error_t ret;

    while (rtcp->active()) {
        start = uvg_rtp::clock::hrc::now();
        ret   = uvg_rtp::poll::poll(rtcp->get_sockets(), buffer, MAX_PACKET, timeout, &nread);

        if (ret == RTP_OK && nread > 0) {
            (void)rtcp->handle_incoming_packet(buffer, (size_t)nread);
        } else if (ret == RTP_INTERRUPTED) {
            /* do nothing */
        } else {
            LOG_ERROR("recvfrom failed, %d", ret);
        }

        diff = uvg_rtp::clock::hrc::diff_now(start);

        if (diff >= MIN_TIMEOUT) {
            if ((ret = rtcp->generate_report()) != RTP_OK && ret != RTP_NOT_READY) {
                LOG_ERROR("Failed to send RTCP status report!");
            }

            timeout = MIN_TIMEOUT;
        }
    }
}
