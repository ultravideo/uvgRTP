#include <uvgrtp/lib.hh>

#include <cstring>
#include <iostream>

/* RTCP (RTP Control Protocol) is used to monitor the quality
 * of the RTP stream. This example demonstrates the usage of
 * sender and receiver reports. RTCP also includes SDES, APP and BYE
 * packets which are not demostrated in this example.
 *
 * This example shows the usage of rtcp while also transmitting RTP
 * stream. The rtcp reports are sent only every 10 seconds and the
 * sender/receiver reports are printed.
*/

constexpr char LOCAL_INTERFACE[] = "127.0.0.1";
constexpr uint16_t LOCAL_PORT = 8888;

constexpr char REMOTE_ADDRESS[] = "127.0.0.1";
constexpr uint16_t REMOTE_PORT = 8890;

constexpr uint16_t PAYLOAD_LEN = 256;
constexpr uint16_t FRAME_RATE = 30;
constexpr uint32_t EXAMPLE_RUN_TIME_S = 30;
constexpr int SEND_TEST_PACKETS = FRAME_RATE*EXAMPLE_RUN_TIME_S;
constexpr int PACKET_INTERVAL_MS = 1000/FRAME_RATE;

/* uvgRTP calls this hook when it receives an RTCP Report
 *
 * NOTE: If application uses hook, it must also free the frame when it's done with i
 * Frame must deallocated using uvgrtp::frame::dealloc_frame() function */
void receiver_hook(uvgrtp::frame::rtcp_receiver_report *frame);
void sender_hook(uvgrtp::frame::rtcp_sender_report *frame);

void wait_until_next_frame(std::chrono::steady_clock::time_point& start, int frame_index);
void cleanup(uvgrtp::context& ctx, uvgrtp::session *local_session, uvgrtp::session *remote_session,
             uvgrtp::media_stream *send, uvgrtp::media_stream *receive);

int main(void)
{
    std::cout << "Starting uvgRTP RTCP hook example" << std::endl;

    // Creation of RTP stream. See sending example for more details
    uvgrtp::context ctx;
    uvgrtp::session *local_session = ctx.create_session(REMOTE_ADDRESS);
    uvgrtp::session *remote_session = ctx.create_session(LOCAL_INTERFACE);

    int flags = RCE_RTCP;
    uvgrtp::media_stream *local_stream = local_session->create_stream(LOCAL_PORT, REMOTE_PORT,
                                                                      RTP_FORMAT_GENERIC, flags);

    uvgrtp::media_stream *remote_stream = remote_session->create_stream(REMOTE_PORT, LOCAL_PORT,
                                                                        RTP_FORMAT_GENERIC, flags);

    // TODO: There is a bug in uvgRTP in how sender reports are implemented and this text reflects
    // that wrong thinking. Sender reports are sent by the sender

    /* In this example code, local_stream acts as the sender and because it is the only sender,
     * it does not send any RTCP frames but only receives RTCP Receiver reports from remote_stream.
     *
     * Because local_stream only sends and remote_stream only receives, we only need to install
     * receive hook for local_stream.
     *
     * By default, all media_stream that have RTCP enabled start as receivers and only if/when they 
     * call push_frame() are they converted into senders. */

    if (!local_stream || local_stream->get_rtcp()->install_receiver_hook(receiver_hook) != RTP_OK)
    {
        std::cerr << "Failed to install RTCP receiver report hook" << std::endl;
        cleanup(ctx, local_session, remote_session, local_stream, remote_stream);
        return EXIT_FAILURE;
    }

    if (!remote_stream || remote_stream->get_rtcp()->install_sender_hook(sender_hook) != RTP_OK)
    {
        std::cerr << "Failed to install RTCP sender report hook" << std::endl;
        cleanup(ctx, local_session, remote_session, local_stream, remote_stream);
        return EXIT_FAILURE;
    }

    if (local_stream)
    {
        // Send dummy data so there's some RTP data to analyze
        uint8_t buffer[PAYLOAD_LEN] = { 0 };
        memset(buffer, 'a', PAYLOAD_LEN);

        memset(buffer,     0, 3);
        memset(buffer + 3, 1, 1);
        memset(buffer + 4, 1, (19 << 1)); // Intra frame

        std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();

        for (unsigned int i = 0; i < SEND_TEST_PACKETS; ++i)
        {
            if ((i+1)%10  == 0 || i == 0) // print every 10 frames and first
            {
                std::cout << "Sending RTP frame " << (i + 1) << "/" << SEND_TEST_PACKETS
                          << " Total data sent: " << (i + 1)*PAYLOAD_LEN << std::endl;
            }

            local_stream->push_frame((uint8_t *)buffer, PAYLOAD_LEN, RTP_NO_FLAGS);

            // send frames at constant interval to mimic a real camera stream
            wait_until_next_frame(start, i);
        }

        std::cout << "Sending finished, total time: " <<
                     (std::chrono::steady_clock::now() - start).count()/1000000 <<" ms" << std::endl;
    }

    cleanup(ctx, local_session, remote_session, local_stream, remote_stream);
    return EXIT_SUCCESS;
}

void receiver_hook(uvgrtp::frame::rtcp_receiver_report *frame)
{
    std::cout << "RTCP receiver report! ----------"       << std::endl;

    for (auto& block : frame->report_blocks)
    {
        std::cout << "ssrc: "       << block.ssrc     << std::endl;
        std::cout << "fraction: "   << block.fraction << std::endl;
        std::cout << "lost: "       << block.lost     << std::endl;
        std::cout << "last_seq: "   << block.last_seq << std::endl;
        std::cout << "jitter: "     << block.jitter   << std::endl;
        std::cout << "lsr: "        << block.lsr      << std::endl;
        std::cout << "dlsr (jiffies): "  << uvgrtp::clock::jiffies_to_ms(block.dlsr)
                  << std::endl << std::endl;
    }

    /* RTCP frames can be deallocated using delete */
    delete frame;
}

void sender_hook(uvgrtp::frame::rtcp_sender_report *frame)
{
    std::cout << "RTCP sender report! ----------"       << std::endl;
    std::cout << "NTP msw: "        << frame->sender_info.ntp_msw   << std::endl;
    std::cout << "NTP lsw: "        << frame->sender_info.ntp_lsw   << std::endl;
    std::cout << "RTP timestamp: "  << frame->sender_info.rtp_ts    << std::endl;
    std::cout << "packet count: "   << frame->sender_info.pkt_cnt   << std::endl;
    std::cout << "byte count: "     << frame->sender_info.byte_cnt  << std::endl;

    for (auto& block : frame->report_blocks)
    {
        std::cout << "ssrc: "       << block.ssrc     << std::endl;
        std::cout << "fraction: "   << block.fraction << std::endl;
        std::cout << "lost: "       << block.lost     << std::endl;
        std::cout << "last_seq: "   << block.last_seq << std::endl;
        std::cout << "jitter: "     << block.jitter   << std::endl;
        std::cout << "lsr: "        << block.lsr      << std::endl;
        std::cout << "dlsr (jiffies): "  << uvgrtp::clock::jiffies_to_ms(block.dlsr)
                  << std::endl << std::endl;
    }

    /* RTCP frames can be deallocated using delete */
    delete frame;
}

void wait_until_next_frame(std::chrono::steady_clock::time_point &start, int frame_index)
{
  // wait until it is time to send the next frame. Simulates a steady sending pace
  // and included only for demostration purposes since you can use uvgRTP to send
  // packets as fast as desired
  auto time_since_start = std::chrono::steady_clock::now() - start;
  auto next_frame_time = (frame_index + 1)*std::chrono::milliseconds(PACKET_INTERVAL_MS);
  if (next_frame_time > time_since_start)
  {
      std::this_thread::sleep_for(next_frame_time - time_since_start);
  }
}

void cleanup(uvgrtp::context &ctx, uvgrtp::session *local_session, uvgrtp::session *remote_session,
             uvgrtp::media_stream *send, uvgrtp::media_stream *receive)
{
  if (send)
  {
      local_session->destroy_stream(send);
  }

  if (receive)
  {
      remote_session->destroy_stream(receive);
  }

  if (local_session)
  {
      // Session must be destroyed manually
      ctx.destroy_session(local_session);
  }

  if (remote_session)
  {
      // Session must be destroyed manually
      ctx.destroy_session(remote_session);
  }
}

