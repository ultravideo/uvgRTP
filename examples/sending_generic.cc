#include <uvgrtp/lib.hh>

#include <climits>
#include <iostream>

/* Generic sending API means, that the user takes the responsibility
 * for RTP payload format. uvgRTP does help a little bit by offering
 * fragmentation function for sending. This means that that if the
 * packet is larger than the specified RTP payload size (default is 1500)
 * the packets are fragmented. This is useful with raw audio of high quality,
 * but unfortenately uvgRTP does not offer full raw-format support.
 * Contributions are welcome.
 *
 * This example demonstrates both sending and receiving of a generic stream. */


// network parameters of the example
constexpr char REMOTE_ADDRESS[] = "127.0.0.1";
constexpr uint16_t REMOTE_PORT = 8888;

// demonstration parameters of the example
constexpr uint32_t PAYLOAD_MAXLEN = (0xffff - 0x1000);
constexpr int TEST_PACKETS = 100;

void rtp_receive_hook(void *arg, uvgrtp::frame::rtp_frame *frame);
void cleanup(uvgrtp::context& ctx, uvgrtp::session *local_session, uvgrtp::session *remote_session,
             uvgrtp::media_stream *send, uvgrtp::media_stream *receive);

int main(void)
{
    std::cout << "Starting uvgRTP generic RTP payload sending example" << std::endl;

    // See sending example for more details
    uvgrtp::context ctx;
    uvgrtp::session *local_session = ctx.create_session(REMOTE_ADDRESS); // REMOTE_ADDRESS will be intereted as remote address due to RCE_SEND_ONLY
    uvgrtp::session *remote_session = ctx.create_session(REMOTE_ADDRESS); // REMOTE_ADDRESS will be interpreted as local address due to RCE_RECEIVE_ONLY

    /* To enable interoperability between RTP libraries, uvgRTP won't fragment generic frames by default.
     *
     * If fragmentation for sender and defragmentation for receiver should be enabled,
     * RCE_FRAGMENT_GENERIC flag must be passed to create_stream()
     *
     * This flag will split the input frame into packets of 1500 bytes and set the marker bit
     * for first and last fragment. When the receiver notices a generic frame with a marker bit
     * set, it knows that the RTP frame is in fact a fragment and when all fragments have been
     * received, uvgRTP constructs one full RTP frame from the fragments and returns the frame to user.
     *
     * See sending.cc for more details about create_stream() */

    int send_flags = RCE_FRAGMENT_GENERIC | RCE_SEND_ONLY;
    int receive_flags = RCE_FRAGMENT_GENERIC | RCE_RECEIVE_ONLY;

    // set only one port, this one port is interpreted based on rce flags
    uvgrtp::media_stream *send = local_session->create_stream(REMOTE_PORT, RTP_FORMAT_GENERIC, send_flags);
    uvgrtp::media_stream *recv = remote_session->create_stream(REMOTE_PORT, RTP_FORMAT_GENERIC, receive_flags);

    if (!recv || recv->install_receive_hook(nullptr, rtp_receive_hook) != RTP_OK)
    {
        cleanup(ctx, local_session, remote_session, send, recv);
        std::cerr << "Failed to install RTP receive hook!" << std::endl;
        return EXIT_FAILURE;
    }

    if (send)
    {
      /* Notice that PAYLOAD_MAXLEN > MTU (4096 > 1500).
     *
     * uvgRTP fragments all generic input frames that are larger than 1500 and in the receiving end,
     * it will reconstruct the full sent frame from fragments when all fragments have been received */
      auto media = std::unique_ptr<uint8_t[]>(new uint8_t[PAYLOAD_MAXLEN]);

      srand(time(NULL));

      for (int i = 0; i < TEST_PACKETS; ++i)
      {
          int random_packet_size = (rand() % PAYLOAD_MAXLEN) + 1;

          for (int i = 0; i < random_packet_size; ++i)
          {
              media[i] = (i + random_packet_size) % CHAR_MAX;
          }

          std::cout << "Sending RTP frame " << i + 1 << '/' << TEST_PACKETS
                    << ". Payload size: " << random_packet_size << std::endl;

          if (send->push_frame(media.get(), random_packet_size, RTP_NO_FLAGS) != RTP_OK)
          {
              cleanup(ctx, local_session, remote_session, send, recv);
              std::cerr << "Failed to send frame!" << std::endl;
              return EXIT_FAILURE;
          }
      }
    }

    /* Session must be destroyed manually */
    ctx.destroy_session(local_session);
    ctx.destroy_session(remote_session);

    return EXIT_SUCCESS;
}

void rtp_receive_hook(void *arg, uvgrtp::frame::rtp_frame *frame)
{
    std::cout << "Received RTP frame. Payload size: " << frame->payload_len << std::endl;

    /* Now we own the frame. Here you could give the frame to the application
     * if f.ex "arg" was some application-specific pointer
     *
     * arg->copy_frame(frame) or whatever
     *
     * When we're done with the frame, it must be deallocated manually */
    (void)uvgrtp::frame::dealloc_frame(frame);
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

