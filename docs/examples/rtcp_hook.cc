#include <uvgrtp/lib.hh>
#include <cstring>


constexpr char LOCAL_INTERFACE[] = "127.0.0.1";
constexpr uint16_t LOCAL_PORT = 8888;

constexpr char REMOTE_ADDRESS[] = "127.0.0.1";
constexpr uint16_t REMOTE_PORT = 8890;

constexpr uint16_t PAYLOAD_MAXLEN = 256;
constexpr uint16_t FRAME_RATE = 30;
constexpr int SEND_TEST_PACKETS = FRAME_RATE*60; // one minute
constexpr int PACKET_INTERVAL_MS = 1000/FRAME_RATE;

/* uvgRTP calls this hook when it receives an RTCP Receiver Report
 *
 * NOTE: If application uses hook, it must also free the frame when it's done with i
 * Frame must deallocated using uvgrtp::frame::dealloc_frame() function */
void receiver_hook(uvgrtp::frame::rtcp_receiver_report *frame)
{
    LOG_INFO("Received an RTCP Receiver Report");

    for (auto& block : frame->report_blocks)
    {
        std::cout << "ssrc: "       << block.ssrc     << std::endl;
        std::cout << "fraction: "   << block.fraction << std::endl;
        std::cout << "lost: "       << block.lost     << std::endl;
        std::cout << "last_seq: "   << block.last_seq << std::endl;
        std::cout << "jitter: "     << block.jitter   << std::endl;
        std::cout << "lsr: "        << block.lsr      << std::endl;
        std::cout << "dlsr (ms): "  << uvgrtp::clock::jiffies_to_ms(block.dlsr) << std::endl;
    }

    /* RTCP frames can be deallocated using delete */
    delete frame;
}

/* uvgRTP calls this hook when it receives an RTCP Sender Report
 *
 * NOTE: If application uses hook, it must also free the frame when it's done with i
 * Frame must deallocated using uvgrtp::frame::dealloc_frame() function */
void sender_hook(uvgrtp::frame::rtcp_sender_report *frame)
{
    LOG_INFO("Received an RTCP Sender Report, Sender Info");

    std::cout << "NTP msw: "        << frame->sender_info.ntp_msw   << std::endl;
    std::cout << "NTP lsw: "        << frame->sender_info.ntp_lsw   << std::endl;
    std::cout << "RTP timestamp: "  << frame->sender_info.rtp_ts    << std::endl;
    std::cout << "packet count: "   << frame->sender_info.pkt_cnt   << std::endl;
    std::cout << "byte count: "     << frame->sender_info.byte_cnt  << std::endl;

    LOG_INFO("Received an RTCP Sender Report, Report blocks");
    for (auto& block : frame->report_blocks)
    {
        std::cout << "ssrc: "       << block.ssrc     << std::endl;
        std::cout << "fraction: "   << block.fraction << std::endl;
        std::cout << "lost: "       << block.lost     << std::endl;
        std::cout << "last_seq: "   << block.last_seq << std::endl;
        std::cout << "jitter: "     << block.jitter   << std::endl;
        std::cout << "lsr: "        << block.lsr      << std::endl;
        std::cout << "dlsr (ms): "  << uvgrtp::clock::jiffies_to_ms(block.dlsr) << std::endl;
    }

    /* RTCP frames can be deallocated using delete */
    delete frame;
}

int main(void)
{
    std::cout << "Starting uvgRTP RTCP hook example" << std::endl;

    /* See sending.cc for more details */
    uvgrtp::context ctx;

    /* See sending.cc for more details */
    uvgrtp::session *local_session = ctx.create_session(REMOTE_ADDRESS);
    uvgrtp::session *remote_session = ctx.create_session(LOCAL_INTERFACE);

    int flags = RCE_RTCP;
    uvgrtp::media_stream *local_stream = local_session->create_stream(LOCAL_PORT, REMOTE_PORT,
                                                                      RTP_FORMAT_GENERIC, flags);

    uvgrtp::media_stream *remote_stream = remote_session->create_stream(REMOTE_PORT, LOCAL_PORT,
                                                                        RTP_FORMAT_GENERIC, flags);

    /* In this example code, local_stream acts as the sender and because it is the only sender,
     * it does not send any RTCP frames but only receives RTCP Receiver reports from remote_stream.
     *
     * Because local_stream only sends and remote_stream only receives, we only need to install
     * receive hook for local_stream.
     *
     * By default, all media_stream that have RTCP enabled start as receivers and only if/when they 
     * call push_frame() are they converted into senders. */

    if (local_stream)
    {
        (void)local_stream->get_rtcp()->install_receiver_hook(receiver_hook);
    }

    if (remote_stream)
    {
        (void)remote_stream->get_rtcp()->install_sender_hook(sender_hook);
    }

    if (local_stream)
    {
        /* Send dummy data so there's some RTCP data to send */
        uint8_t buffer[PAYLOAD_MAXLEN] = { 0 };
        memset(buffer, 'a', PAYLOAD_MAXLEN);

        auto start = std::chrono::steady_clock::now();

        for (unsigned int i = 0; i < SEND_TEST_PACKETS; ++i)
        {
            std::cout << "Sending RTP frame " << (i + 1) << "/" << SEND_TEST_PACKETS
                      << " Total data sent: " << (i + 1)*PAYLOAD_MAXLEN << std::endl;

            local_stream->push_frame((uint8_t *)buffer, PAYLOAD_MAXLEN, RTP_NO_FLAGS);

            // wait until it is time to send the next frame. Simulates a steady sending pace
            // and included only for demostration purposes since you can use uvgRTP to send
            // packets as fast as desired
            auto time_since_start = std::chrono::steady_clock::now() - start;
            auto next_frame_time = (i + 1)*std::chrono::milliseconds(PACKET_INTERVAL_MS);
            if (next_frame_time > time_since_start)
            {
                std::this_thread::sleep_for(next_frame_time - time_since_start);
            }
        }

        std::cout << "Sending finished, total time: " <<
                     (std::chrono::steady_clock::now() - start).count()/1000000 <<" ms" << std::endl;

        local_session->destroy_stream(local_stream);
    }

    if (remote_stream)
    {
        remote_session->destroy_stream(remote_stream);
    }

    if (local_session)
    {
        ctx.destroy_session(local_session);
    }

    if (remote_session)
    {
        ctx.destroy_session(remote_session);
    }
}
