#include <uvgrtp/lib.hh>
#include <climits>
#include <cstring>

#include <iostream>
#include <cstring>

/* Zimmermann RTP (ZRTP) is a key management protocol for SRTP. Compared
 * to most approaches, using ZRTP can facilitate end-to-end encryption
 * of media traffic since the keys are exchanged peer-to-peer.
 *
 * Using ZRTP in uvgRTP requires only setting it on with RCE_SRTP_KMNGMNT_ZRTP
 * flag. Then when creating the media streams, you will encounter a small additional
 * wait until the ZRTP negotiation has been completed. ZRTP has to only be negotiatiated
 * once per session, since the following media_streams can use the key context from
 * the first media_stream.
 *
 * This example demonstrates usign the ZRTP to negotiate SRTP encryption context
 * for multiple media_streams. There are two senders and two receivers representing
 * video and audio streams.
 */

// Network parameters of this example
constexpr char SENDER_ADDRESS[] = "127.0.0.1";
constexpr uint16_t SENDER_VIDEO_PORT = 8888;
constexpr uint16_t SENDER_AUDIO_PORT = 8890;

constexpr char RECEIVER_ADDRESS[] = "127.0.0.1";
constexpr uint16_t RECEIVER_VIDEO_PORT = 7776;
constexpr uint16_t RECEIVER_AUDIO_PORT = 7778;

// demonstration parameters of this example
constexpr int VIDEO_PAYLOAD_SIZE = 4000;
constexpr int AUDIO_PAYLOAD_SIZE = 100;

constexpr auto EXAMPLE_RUN_TIME_S = std::chrono::seconds(2);
constexpr auto RECEIVER_WAIT_TIME_MS = std::chrono::milliseconds(50);

constexpr auto AUDIO_FRAME_INTERVAL_MS = std::chrono::milliseconds(20);

constexpr auto VIDEO_FRAME_INTERVAL_MS = std::chrono::milliseconds(1000/60); // 60 fps video

void receive_function(uvgrtp::session* receiver_session, int flags, std::shared_ptr<std::mutex> print_mutex,
                      RTP_FORMAT format, uint16_t receiver_port, uint16_t sender_port);
void sender_function(uvgrtp::session* sender_session, int flags, std::shared_ptr<std::mutex> print_mutex,
                     RTP_FORMAT format, uint16_t sender_port, uint16_t receiver_port, size_t payload_size,
                     std::chrono::milliseconds frame_interval);
void wait_until_next_frame(std::chrono::steady_clock::time_point& start, std::chrono::milliseconds interval, int frame_index);

int main(void)
{
    std::cout << "Starting uvgRTP SRTP together with ZRTP example" << std::endl;

    uvgrtp::context receiver_ctx;

    // check that Crypto++ has been compiled into uvgRTP, otherwise encryption wont work.
    if (!receiver_ctx.crypto_enabled())
    {
        std::cerr << "Cannot run SRTP example if crypto++ is not included in uvgRTP!"
                  << std::endl;
        return EXIT_FAILURE;
    }

    std::cout << "Initializing receivers" << std::endl;
    uvgrtp::session *receiver_session = receiver_ctx.create_session(SENDER_ADDRESS, RECEIVER_ADDRESS);

    std::shared_ptr<std::mutex> print_mutex = std::shared_ptr<std::mutex> (new std::mutex);

    /* Create separate thread for the receiver
     *
     * Because we're using ZRTP for SRTP key management,
     * the receiver and sender must communicate with each other
     * before the actual media communication starts */

    // Enable SRTP and use ZRTP to manage keys for both sender and receiver*/
    unsigned rce_dh_flags          = RCE_SRTP | RCE_SRTP_KMNGMNT_ZRTP | RCE_ZRTP_DIFFIE_HELLMAN_MODE;
    unsigned rce_multistream_flags = RCE_SRTP | RCE_SRTP_KMNGMNT_ZRTP | RCE_ZRTP_MULTISTREAM_MODE;

    // start the receivers in a separate thread
    std::thread a_receiver(receive_function, receiver_session, rce_dh_flags, print_mutex,
                           RTP_FORMAT_OPUS, RECEIVER_AUDIO_PORT, SENDER_AUDIO_PORT);

    std::thread v_receiver(receive_function, receiver_session, rce_multistream_flags, print_mutex,
                           RTP_FORMAT_H265, RECEIVER_VIDEO_PORT, SENDER_VIDEO_PORT);


    std::cout << "Initializing senders" << std::endl;
    uvgrtp::context sender_ctx;
    uvgrtp::session *sender_session = sender_ctx.create_session(RECEIVER_ADDRESS, SENDER_ADDRESS);

    // start the senders in their own threads
    std::thread a_sender(sender_function, sender_session, rce_dh_flags, print_mutex,
                         RTP_FORMAT_OPUS, SENDER_AUDIO_PORT, RECEIVER_AUDIO_PORT,
                         AUDIO_PAYLOAD_SIZE, AUDIO_FRAME_INTERVAL_MS);

    std::thread v_sender(sender_function, sender_session, rce_multistream_flags, print_mutex,
                         RTP_FORMAT_H265, SENDER_VIDEO_PORT, RECEIVER_VIDEO_PORT,
                         VIDEO_PAYLOAD_SIZE, VIDEO_FRAME_INTERVAL_MS);

    // wait until all threads have ended
    if (a_receiver.joinable())
    {
        a_receiver.join();
    }
    if (v_receiver.joinable())
    {
        v_receiver.join();
    }
    if (a_sender.joinable())
    {
        a_sender.join();
    }
    if (v_sender.joinable())
    {
        v_sender.join();
    }

    if (sender_session)
        sender_ctx.destroy_session(sender_session);

    if (receiver_session)
        sender_ctx.destroy_session(receiver_session);

    std::cout << "ZRTP example finished" << std::endl;

    return EXIT_SUCCESS;
}

void receive_function(uvgrtp::session* receiver_session, int flags,
                      std::shared_ptr<std::mutex> print_mutex,
                      RTP_FORMAT format, uint16_t receiver_port, uint16_t sender_port)
{
    print_mutex->lock();
    std::cout << "Receiver thread port: " << receiver_port << "<-" << sender_port << std::endl;
    print_mutex->unlock();
    /* Keys created using Multistream mode */
    uvgrtp::media_stream *receiver_stream =
        receiver_session->create_stream(receiver_port, sender_port, format, flags);

    if (receiver_stream)
    {
        uvgrtp::frame::rtp_frame *frame = nullptr;

        std::cout << "Start receiving frames for " << EXAMPLE_RUN_TIME_S.count()
                  << " seconds" << std::endl;
        auto start = std::chrono::steady_clock::now();

        while (std::chrono::steady_clock::now() - start < EXAMPLE_RUN_TIME_S)
        {
            /* You can specify a timeout for the operation and if the a frame is not received
             * within that time limit, pull_frame() returns a nullptr
             *
             * The parameter tells how long time a frame is waited in milliseconds */

            frame = receiver_stream->pull_frame(RECEIVER_WAIT_TIME_MS.count());

            if (frame)
            {
                print_mutex->lock();
                std::cout << "Received a frame. Sequence number: " << frame->header.seq << std::endl;
                print_mutex->unlock();

                // Process the frame here

                (void)uvgrtp::frame::dealloc_frame(frame);
            }
        }

        receiver_session->destroy_stream(receiver_stream);
    }
}

void sender_function(uvgrtp::session* sender_session, int flags, std::shared_ptr<std::mutex> print_mutex,
                     RTP_FORMAT format, uint16_t sender_port, uint16_t receiver_port, size_t payload_size,
                     std::chrono::milliseconds frame_interval)
{
    print_mutex->lock();
    std::cout << "Sender thread port: " << sender_port << "->" << receiver_port << std::endl;
    print_mutex->unlock();

    /* The first call to create_stream() creates keys for the session using Diffie-Hellman
     * key exchange and all subsequent calls to create_stream() initialize keys for the
     * stream using Multistream mode */
    uvgrtp::media_stream *sender_audio_strm = sender_session->create_stream(sender_port,
                                                                            receiver_port,
                                                                            format, flags);

    if (sender_audio_strm)
    {
        auto start = std::chrono::steady_clock::now();

        for (int i = 0; std::chrono::steady_clock::now() < (start + EXAMPLE_RUN_TIME_S); ++i)
        {
            /*
            print_mutex->lock();
            std::cout << "Sending frame" << std::endl;
            print_mutex->unlock();
            */

            std::unique_ptr<uint8_t[]> dummy_frame = std::unique_ptr<uint8_t[]>(new uint8_t[payload_size]);

            if (format == RTP_FORMAT_H265 && payload_size >= 5)
            {
              memset(dummy_frame.get(), 'a', payload_size); // data
              memset(dummy_frame.get(),     0, 3);
              memset(dummy_frame.get() + 3, 1, 1);
              memset(dummy_frame.get() + 4, 1, (19 << 1)); // Intra frame NAL type
            }

            if (sender_audio_strm->push_frame(std::move(dummy_frame), payload_size, RTP_NO_FLAGS) != RTP_OK)
            {
                std::cerr << "Failed to send frame" << std::endl;
            }

            // wait until it is time to send the next frame. Included only for
            // demostration purposes since you can use uvgRTP to send packets as fast as desired
            wait_until_next_frame(start, frame_interval, i);
        }
    }
}

void wait_until_next_frame(std::chrono::steady_clock::time_point& start,
                           std::chrono::milliseconds interval, int frame_index)
{
  // wait until it is time to send the next frame. Simulates a steady sending pace
  // and included only for demostration purposes since you can use uvgRTP to send
  // packets as fast as desired
  auto time_since_start = std::chrono::steady_clock::now() - start;
  auto next_frame_time = (frame_index + 1)*interval;
  if (next_frame_time > time_since_start)
  {
      std::this_thread::sleep_for(next_frame_time - time_since_start);
  }
}
