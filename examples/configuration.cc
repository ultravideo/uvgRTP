#include <uvgrtp/lib.hh>

#include <iostream>
#include <cstring>

/* This example demonstrates using the configuration options of uvgRTP.
 * There are three types of configuration flags: RCE, RCC and RTP flags
 * RCE (RTP Context Enable) flags are used to enable different features
 * of uvgRTP and are passed when a new media_stream is created.
 *
 * RCC (RTP Context Configuration) flags can be used to modify the behavior of media
 * stream. They are used by calling configure_ctx-function of media_stream
 * and using the flag and value as parameters.
 *
 * Lastly, RTP flags can be added to modify the sending process of uvgRTP.
*/


/* This example implements one sender and one receiver.
 * These are their used interfaces and ports. You may
 * edit these if you wish to test this example on different machines */
constexpr char LOCAL_ADDRESS[] = "127.0.0.1";
constexpr uint16_t LOCAL_PORT = 8888;

constexpr char REMOTE_ADDRESS[] = "127.0.0.1";
constexpr uint16_t REMOTE_PORT = 8890;


// parameters for this example
constexpr int BUFFER_SIZE_MB = 40 * 1000 * 1000;
constexpr int MAX_PACKET_INTERVAL_MS = 150;

constexpr size_t PAYLOAD_LEN = 4096;
constexpr int SEND_TEST_PACKETS = 1000;

void receive_process_hook(void *arg, uvgrtp::frame::rtp_frame *frame);
void cleanup(uvgrtp::context& ctx, uvgrtp::session *local_session, uvgrtp::session *remote_session,
             uvgrtp::media_stream *send, uvgrtp::media_stream *receive);


int main(void)
{
    std::cout << "Starting uvgRTP configuration example" << std::endl;

    /* Some of the functionality of uvgRTP can be enabled or disabled using RCE_* flags.
     *
     * For example, here the created MediaStream object has RTCP enabled,
     * does not utilize system call clustering to reduce the possibility of packet dropping */
    int send_flags =
        RCE_RTCP |                      /* enable RTCP */
        RCE_SYSTEM_CALL_CLUSTERING |    /* Enable system call clustering (only Linux) */
        RCE_SEND_ONLY;                  /* interpret address/port as destination address/port */

    /* Prepends a 4-byte HEVC start code (0x00000001) before each NAL unit.
     * This way the stream can be saved into a file and played by a media player */
    int receive_flags =
        RCE_RTCP |                      /* enable RTCP */
        RCE_RECEIVE_ONLY;               /* interpret address/port as binding interface */

    uvgrtp::context ctx;
    uvgrtp::session *local_session = ctx.create_session(REMOTE_ADDRESS, LOCAL_ADDRESS);
    uvgrtp::media_stream *send = local_session->create_stream(REMOTE_PORT, LOCAL_PORT, RTP_FORMAT_H265, send_flags);

    uvgrtp::session *remote_session = ctx.create_session(LOCAL_ADDRESS, REMOTE_ADDRESS);
    uvgrtp::media_stream *receive = remote_session->create_stream(LOCAL_PORT, REMOTE_PORT, RTP_FORMAT_H265, receive_flags);

    if (receive)
    {
        /* uvgRTP context can also be configured using RCC_* flags
         * These flags do not enable/disable functionality but alter default behaviour of uvgRTP
         *
         * For example, here UDP receive buffer is increased to BUFFER_SIZE_MB
         * and frame delay is set PACKET_MAX_DELAY_MS to allow frames to arrive a little late */
          receive->configure_ctx(RCC_UDP_RCV_BUF_SIZE, BUFFER_SIZE_MB);
          receive->configure_ctx(RCC_RING_BUFFER_SIZE, BUFFER_SIZE_MB);
          receive->configure_ctx(RCC_PKT_MAX_DELAY,    MAX_PACKET_INTERVAL_MS);

          // set the MTU size of expected packets
          receive->configure_ctx(RCC_MTU_SIZE, 1400);

          // Change the payload number used in RTP header
          receive->configure_ctx(RCC_DYN_PAYLOAD_TYPE, 120);

          // install receive hook for asynchronous reception
          receive->install_receive_hook(nullptr, receive_process_hook);
    }
    else
    {
        std::cerr << "Failed to create the receiver!" << std::endl;
        cleanup(ctx, local_session, remote_session, send, receive);
        return EXIT_FAILURE;
    }

    if (send)
    {
        /* Here, the UDP send buffer is increased to BUFFER_SIZE_MB */
        send->configure_ctx(RCC_UDP_SND_BUF_SIZE, BUFFER_SIZE_MB);

        receive->configure_ctx(RCC_DYN_PAYLOAD_TYPE, 120);

        /* Set the MTU size to what you expect the network to support 
           (uvgRTP substracts UDP and IP headers from this number) */
        send->configure_ctx(RCC_MTU_SIZE, 1400);

        for (int i = 0; i < SEND_TEST_PACKETS; ++i)
        {
            auto buffer = std::unique_ptr<uint8_t[]>(new uint8_t[PAYLOAD_LEN]);
            memset(buffer.get(), 'a', PAYLOAD_LEN);
            memset(buffer.get(),     0, 3);
            memset(buffer.get() + 3, 1, 1);
            memset(buffer.get() + 4, 1, (19 << 1)); // Intra frame

            if ((i+1)%10  == 0 || i == 0) // print every 10 frames and first
                std::cout << "Sending frame " << i + 1 << '/' << SEND_TEST_PACKETS << std::endl;

            if (send->push_frame(std::move(buffer), PAYLOAD_LEN, RTP_NO_FLAGS) != RTP_OK)
            {
                std::cerr << "Failed to send RTP frame!" << std::endl;
                cleanup(ctx, local_session, remote_session, send, receive);
                return EXIT_FAILURE;
            }
        }
    }

    cleanup(ctx, local_session, remote_session, send, receive);

    return EXIT_SUCCESS;
}

void receive_process_hook(void *arg, uvgrtp::frame::rtp_frame *frame)
{
    std::cout << "Received frame. Payload size: " << frame->payload_len << std::endl;
    uvgrtp::frame::dealloc_frame(frame);
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
