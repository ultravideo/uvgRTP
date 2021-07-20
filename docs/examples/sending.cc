#include <uvgrtp/lib.hh>

#include <iostream>

constexpr uint16_t LOCAL_PORT = 8888;

constexpr char REMOTE_ADDRESS[] = "127.0.0.1";
constexpr uint16_t REMOTE_PORT = 8890;

constexpr size_t PAYLOAD_LEN = 100;
constexpr int SEND_TEST_PACKETS = 100;

constexpr auto END_WAIT = std::chrono::seconds(5);

int main(void)
{
    std::cout << "Starting uvgRTP RTP sending example" << std::endl;

    /* To use the library, one must create a global RTP context object */
    uvgrtp::context ctx;

    /* Each new IP address requires a separate RTP session */
    uvgrtp::session *sess = ctx.create_session(REMOTE_ADDRESS);

    /* Each RTP session has one or more media streams. These media streams are bidirectional
     * and they require both source and destination ports for the connection. One must also
     * specify the media format for the stream and any configuration flags if needed.
     *
     * See configuration.cc for more details about configuration.
     *
     * First port is source port aka the port that we listen to and second port is the port
     * that remote listens to
     *
     * This same object is used for both sending and receiving media
     *
     * In this example, we have one media stream with the remote participant: H265 */

    int flags = RTP_NO_FLAGS;
    uvgrtp::media_stream *hevc = sess->create_stream(LOCAL_PORT, REMOTE_PORT,
                                                     RTP_FORMAT_H265, flags);

    if (hevc)
    {
      for (int i = 0; i < SEND_TEST_PACKETS; ++i)
      {
          std::unique_ptr<uint8_t[]> dummy_frame = std::unique_ptr<uint8_t[]>(new uint8_t[PAYLOAD_LEN]);
          std::cout << "Sending frame " << i + 1 << '/' << SEND_TEST_PACKETS << std::endl;

          if (hevc->push_frame(std::move(dummy_frame), PAYLOAD_LEN, RTP_NO_FLAGS) != RTP_OK)
          {
              std::cout << "Failed to send RTP frame!" << std::endl;
          }
      }

       std::cout << "Sending finished. Waiting "<< END_WAIT.count()
                 << " seconds before exiting." << std::endl;

      std::this_thread::sleep_for(END_WAIT);

      sess->destroy_stream(hevc);
    }

    if (sess)
    {
        /* Session must be destroyed manually */
        ctx.destroy_session(sess);
    }

    return EXIT_SUCCESS;
}
