#include <uvgrtp/lib.hh>

#include <iostream>
#include <cstring>

/* Some NATs may close the hole created in the firewall if the stream is not bidirectional,
 * i.e., only one participant produces and the other consumes.
 *
 * To prevent the connection from closing, uvgRTP can be instructed to keep the hole open
 * by periodically sending 1-byte datagram to remote (once every 2 seconds).
 *
 * All RFC 3550 compatible implementations should ignore the packet as it is not recognized
 * to be a valid RTP frame and the stream should work without problems.
 *
 * This feature is enabled by giving RCE_HOLEPUNCH_KEEPALIVE flag to the unidirectional
 * media_stream that acts as the receiver. Please note that this flag is only necessary
 * if you're using the created media_stream object as a unidirectional stream and you are
 * noticing that after a while the packets are no longer passing through the firewall

 * In this example, we demonstrate the functionality of RCE_HOLEPUNCH_KEEPALIVE by sending
 * a dummy stream from sender to receiver with hole punching feature enabled by configuration
 * flag of the receiver.
 */

// network parameters of the example
constexpr char LOCAL_INTERFACE[] = "127.0.0.1";
constexpr uint16_t LOCAL_PORT = 8888;
constexpr char REMOTE_ADDRESS[] = "127.0.0.1";
constexpr uint16_t REMOTE_PORT = 8890;

// Parameters of sent dummy frames
constexpr uint16_t PAYLOAD_LEN = 256; // how large are test packets
constexpr int AMOUNT_OF_PACKETS = 100; // how many
constexpr int PACKET_INTERVAL_MS = 1000/30; // how often

// Function where received frames are processed
void frame_process_hook(void *arg, uvgrtp::frame::rtp_frame *frame);
void wait_until_next_frame(std::chrono::steady_clock::time_point& start, int frame_index);
void cleanup(uvgrtp::context& rtp_ctx,
             uvgrtp::session *sending_session, uvgrtp::session *receiving_session,
             uvgrtp::media_stream *send, uvgrtp::media_stream *recv);

int main(void)
{
    std::cout << "Starting uvgRTP binding example" << std::endl;

    uvgrtp::context rtp_ctx;
    uvgrtp::session *sending_session = rtp_ctx.create_session(LOCAL_INTERFACE, REMOTE_ADDRESS);
    uvgrtp::media_stream *send = sending_session->create_stream(LOCAL_PORT, REMOTE_PORT,
                                                                RTP_FORMAT_H265, RCE_NO_FLAGS);

    /* RCE flags or RTP Context Enable flags are given when creating the Media Stream.
       Notice the RCE_HOLEPUNCH_KEEPALIVE flag which keeps the NAT/firewall open */
    int flags = RCE_HOLEPUNCH_KEEPALIVE;
    uvgrtp::session *receiving_session = rtp_ctx.create_session(REMOTE_ADDRESS, LOCAL_INTERFACE);
    uvgrtp::media_stream *recv = receiving_session->create_stream(REMOTE_PORT, LOCAL_PORT,
                                                                  RTP_FORMAT_H265, flags);

    // install receive hook for asynchronous reception
    if (!recv || recv->install_receive_hook(nullptr, frame_process_hook) != RTP_OK)
    {
        std::cerr << "Failed to install receive hook!" << std::endl;
        cleanup(rtp_ctx, sending_session, receiving_session, send, recv);
        return EXIT_FAILURE;
    }

    if (send)
    {
        auto start = std::chrono::steady_clock::now();
        for (unsigned int i = 0; i < AMOUNT_OF_PACKETS; ++i)
        {
            std::cout << "Sending frame " << i + 1 << '/' << AMOUNT_OF_PACKETS << std::endl;

            // uvgRTP mandates the existance of NAL units so we fake some
            std::unique_ptr<uint8_t[]> dummy_frame =
                std::unique_ptr<uint8_t[]>(new uint8_t[PAYLOAD_LEN]);
            memset(dummy_frame.get(), 'a', PAYLOAD_LEN); // payload
            memset(dummy_frame.get(),     0, 3);
            memset(dummy_frame.get() + 3, 1, 1);
            memset(dummy_frame.get() + 4, 1, (19 << 1)); // Intra frame NAL type

            if (send->push_frame(std::move(dummy_frame), PAYLOAD_LEN, RTP_NO_FLAGS) != RTP_OK)
            {
                std::cerr << "Failed to send frame" << std::endl;
                cleanup(rtp_ctx, sending_session, receiving_session, send, recv);
                return EXIT_FAILURE;
            }

            /* Send frames at constant intervals. This example makes sure the frames are
             * sent exactly at the right time by calculating the timeslots for each frame.
             * If the full data is already available in real life, you can send it as fast
             * as your network can handle, but here we simulate how a 30 fps camera would
             * send frames. */
            wait_until_next_frame(start, i);
        }
    }

    cleanup(rtp_ctx, sending_session, receiving_session, send, recv);

    return EXIT_SUCCESS;
}

void frame_process_hook(void *arg, uvgrtp::frame::rtp_frame *frame)
{
    std::cout << "Received frame. Payload size: " << frame->payload_len << std::endl;

    /* Use the hook function for handing over the frame to other thread.
     * It is not recommended to perform heavy computation in hook function
     * as this may interfere with uvgRTP:s ability to receive frames. */

    uvgrtp::frame::dealloc_frame(frame);
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

void cleanup(uvgrtp::context &rtp_ctx,
             uvgrtp::session *sending_session, uvgrtp::session *receiving_session,
             uvgrtp::media_stream *send, uvgrtp::media_stream *recv)
{
    if (send)
        sending_session->destroy_stream(send);
    if (recv)
        receiving_session->destroy_stream(recv);

    if (sending_session)
        rtp_ctx.destroy_session(sending_session);
    if (receiving_session)
        rtp_ctx.destroy_session(receiving_session);
}
