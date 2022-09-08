#include <uvgrtp/lib.hh>

#include <iostream>
#include <cstring>

/* RTP is a protocol for real-time streaming. The simplest usage
 * scenario is sending one RTP stream and receiving it. This example
 * Shows how to send one RTP stream. These examples perform a simple
 * test if they are run. You may run the receiving examples at the same
 * time to see the whole demo. */

/* parameters of this example. You may change these to reflect
 * you network environment. */
constexpr char REMOTE_ADDRESS[] = "127.0.0.1";
constexpr uint16_t REMOTE_PORT = 8890;

// the parameters of demostration
constexpr size_t PAYLOAD_LEN = 100;
constexpr int    AMOUNT_OF_TEST_PACKETS = 100;
constexpr auto   END_WAIT = std::chrono::seconds(5);

int main(void)
{
    std::cout << "Starting uvgRTP RTP sending example" << std::endl;

    /* To use the library, one must create a global RTP context object */
    uvgrtp::context ctx;

    // A session represents
    uvgrtp::session *sess = ctx.create_session(REMOTE_ADDRESS);

    /* Each RTP session has one or more media streams. These media streams are bidirectional
     * and they require both source and destination ports for the connection. One must also
     * specify the media format for the stream and any configuration flags if needed.
     *
     * See Configuration example for more details about configuration.
     *
     * First port is source port aka the port that we listen to and second port is the port
     * that remote listens to
     *
     * This same object is used for both sending and receiving media
     *
     * In this example, we have one media stream with the remote participant: H265 */

    int flags = RCE_SEND_ONLY;
    uvgrtp::media_stream *hevc = sess->create_stream(REMOTE_PORT, RTP_FORMAT_H265, flags);

    if (hevc)
    {
        /* In this example we send packets as fast as possible. The source can be
         * a file or a real-time encoded stream */
        for (int i = 0; i < AMOUNT_OF_TEST_PACKETS; ++i)
        {
            std::unique_ptr<uint8_t[]> dummy_frame = std::unique_ptr<uint8_t[]>(new uint8_t[PAYLOAD_LEN]);
            memset(dummy_frame.get(), 'a', PAYLOAD_LEN); // NAL payload
            memset(dummy_frame.get(),     0, 3);
            memset(dummy_frame.get() + 3, 1, 1);
            memset(dummy_frame.get() + 4, 1, (19 << 1)); // Intra frame NAL type

            if ((i+1)%10  == 0 || i == 0) // print every 10 frames and first
                std::cout << "Sending frame " << i + 1 << '/' << AMOUNT_OF_TEST_PACKETS << std::endl;

            if (hevc->push_frame(std::move(dummy_frame), PAYLOAD_LEN, RTP_NO_FLAGS) != RTP_OK)
            {
                std::cout << "Failed to send RTP frame!" << std::endl;
            }
        }

         std::cout << "Sending finished. Waiting "<< END_WAIT.count()
                   << " seconds before exiting." << std::endl;

        // wait a little bit so pop-up console users have time to see the results
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
