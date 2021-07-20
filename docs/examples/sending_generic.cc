#include <uvgrtp/lib.hh>
#include <climits>

constexpr char LOCAL_INTERFACE[] = "127.0.0.1";
constexpr uint16_t LOCAL_PORT = 8888;

constexpr char REMOTE_ADDRESS[] = "127.0.0.1";
constexpr uint16_t REMOTE_PORT = 8890;

constexpr uint32_t PAYLOAD_MAXLEN = (0xffff - 0x1000);

constexpr int TEST_PACKETS = 100;

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


int main(void)
{
    std::cout << "Starting uvgRTP generic RTP payload sending example" << std::endl;

    /* See sending.cc for more details */
    uvgrtp::context ctx;

    /* See sending.cc for more details */
    uvgrtp::session *local_session = ctx.create_session(REMOTE_ADDRESS);
    uvgrtp::session *remote_session = ctx.create_session(REMOTE_ADDRESS);

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
    uvgrtp::media_stream *send = local_session->create_stream(LOCAL_PORT, REMOTE_PORT, RTP_FORMAT_GENERIC, RCE_FRAGMENT_GENERIC);
    uvgrtp::media_stream *recv = remote_session->create_stream(REMOTE_PORT, LOCAL_PORT, RTP_FORMAT_GENERIC, RCE_FRAGMENT_GENERIC);

    if (recv)
        recv->install_receive_hook(nullptr, rtp_receive_hook);

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
          int size = (rand() % PAYLOAD_MAXLEN) + 1;

          for (int i = 0; i < size; ++i)
          {
              media[i] = (i + size) % CHAR_MAX;
          }

          std::cout << "Sending RTP frame " << i + 1 << '/' << TEST_PACKETS << ". Payload size: " << size << std::endl;

          if (send->push_frame(media.get(), size, RTP_NO_FLAGS) != RTP_OK) {
              fprintf(stderr, "Failed to send frame!\n");
              return -1;
          }
      }
    }

    /* Session must be destroyed manually */
    ctx.destroy_session(local_session);
    ctx.destroy_session(remote_session);

    return EXIT_SUCCESS;
}
