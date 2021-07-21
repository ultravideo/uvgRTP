#include <uvgrtp/lib.hh>
#include <climits>
#include <cstring>

constexpr char SENDER_ADDRESS[] = "127.0.0.1";
constexpr uint16_t SENDER_VIDEO_PORT = 8888;
constexpr uint16_t SENDER_AUDIO_PORT = 8890;

constexpr char RECEIVER_ADDRESS[] = "127.0.0.1";
constexpr uint16_t RECEIVER_VIDEO_PORT = 7776;
constexpr uint16_t RECEIVER_AUDIO_PORT = 7778;

constexpr int VIDEO_PAYLOAD_SIZE = 4000;
constexpr int AUDIO_PAYLOAD_SIZE = 100;

constexpr auto EXAMPLE_RUN_TIME_S = std::chrono::seconds(10);
constexpr auto RECEIVER_WAIT_TIME_MS = std::chrono::milliseconds(50);

constexpr auto AUDIO_FRAME_INTERVAL_MS = std::chrono::milliseconds(20);

constexpr auto VIDEO_FRAME_INTERVAL_MS = std::chrono::milliseconds(1000/60); // 60 fps video

void receive_function(uvgrtp::session* receiver_session, int flags, std::shared_ptr<std::mutex> print_mutex,
                      RTP_FORMAT format, uint16_t receiver_port, uint16_t sender_port);
void sender_function(uvgrtp::session* sender_session, int flags, std::shared_ptr<std::mutex> print_mutex,
                     RTP_FORMAT format, uint16_t sender_port, uint16_t receiver_port, size_t payload_size,
                     std::chrono::milliseconds frame_interval);

int main(void)
{
    std::cout << "Starting uvgRTP SRTP together with ZRTP example" << std::endl;

    /* Enable SRTP and use ZRTP to manage keys for both sender and receiver*/
    unsigned rce_flags = RCE_SRTP | RCE_SRTP_KMNGMNT_ZRTP;

    uvgrtp::context receiver_ctx;

    if (!receiver_ctx.crypto_enabled())
    {
        std::cerr << "Cannot run SRTP example if crypto is not included in uvgRTP!"
                  << std::endl;
        return EXIT_FAILURE;
    }

    uvgrtp::session *receiver_session = receiver_ctx.create_session(SENDER_ADDRESS);

    std::shared_ptr<std::mutex> print_mutex = std::shared_ptr<std::mutex> (new std::mutex);

    /* Create separate thread for the receiver
     *
     * Because we're using ZRTP for SRTP key management,
     * the receiver and sender must communicate with each other
     * before the actual media communication starts */

    std::thread a_receiver(receive_function, receiver_session, rce_flags, print_mutex,
                           RTP_FORMAT_OPUS, RECEIVER_AUDIO_PORT, SENDER_AUDIO_PORT);

    std::thread v_receiver(receive_function, receiver_session, rce_flags, print_mutex,
                           RTP_FORMAT_H266, RECEIVER_VIDEO_PORT, SENDER_VIDEO_PORT);

    /* See sending.cc for more details */
    uvgrtp::context sender_ctx;
    uvgrtp::session *sender_session = sender_ctx.create_session(RECEIVER_ADDRESS);

    std::thread a_sender(sender_function, sender_session, rce_flags, print_mutex,
                         RTP_FORMAT_OPUS, SENDER_AUDIO_PORT, RECEIVER_AUDIO_PORT,
                         AUDIO_PAYLOAD_SIZE, AUDIO_FRAME_INTERVAL_MS);

    std::thread v_sender(sender_function, sender_session, rce_flags, print_mutex,
                         RTP_FORMAT_H266, SENDER_VIDEO_PORT, RECEIVER_VIDEO_PORT,
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
                std::cout << "Received a frame. Payload size: " << frame->payload_len << std::endl;
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
            print_mutex->lock();
            std::cout << "Sending frame" << std::endl;
            print_mutex->unlock();

            std::unique_ptr<uint8_t[]> dummy_audio_frame = std::unique_ptr<uint8_t[]>(new uint8_t[payload_size]);

            if (sender_audio_strm->push_frame(std::move(dummy_audio_frame), payload_size, RTP_NO_FLAGS) != RTP_OK)
            {
                std::cerr << "Failed to send frame" << std::endl;
            }

            // wait until it is time to send the next frame. Included only for
            // demostration purposes since you can use uvgRTP to send packets as fast as desired
            auto time_since_start = std::chrono::steady_clock::now() - start;
            auto next_frame_time = (i + 1)*std::chrono::milliseconds(frame_interval);
            if (next_frame_time > time_since_start)
            {
                std::this_thread::sleep_for(next_frame_time - time_since_start);
            }
        }
    }
}
