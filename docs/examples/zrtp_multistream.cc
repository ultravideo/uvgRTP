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

void audio_receiver(uvgrtp::session* receiver_session, int flags, std::shared_ptr<std::mutex> print_mutex);
void video_receiver(uvgrtp::session* receiver_session, int flags, std::shared_ptr<std::mutex> print_mutex);

void audio_sender(uvgrtp::session* sender_session, int flags, std::shared_ptr<std::mutex> print_mutex);
void video_sender(uvgrtp::session* sender_session, int flags, std::shared_ptr<std::mutex> print_mutex);



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
    std::thread a_receiver(audio_receiver, receiver_session, rce_flags, print_mutex);
    std::thread v_receiver(video_receiver, receiver_session, rce_flags, print_mutex);

    /* See sending.cc for more details */
    uvgrtp::context sender_ctx;
    uvgrtp::session *sender_session = sender_ctx.create_session(RECEIVER_ADDRESS);

    std::thread a_sender(audio_sender, sender_session, rce_flags, print_mutex);
    std::thread v_sender(video_sender, sender_session, rce_flags, print_mutex);

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


void audio_receiver(uvgrtp::session* receiver_session, int flags, std::shared_ptr<std::mutex> print_mutex)
{
  /* Keys created using Multistream mode */
  uvgrtp::media_stream *receiver_audio_strm =
      receiver_session->create_stream(RECEIVER_AUDIO_PORT, SENDER_AUDIO_PORT,
                                      RTP_FORMAT_GENERIC, flags);

  if (receiver_audio_strm)
  {
      uvgrtp::frame::rtp_frame *frame = nullptr;

      std::cout << "Start receiving audio frames for " << EXAMPLE_RUN_TIME_S.count()
                << " seconds" << std::endl;
      auto start = std::chrono::steady_clock::now();

      while (std::chrono::steady_clock::now() - start < EXAMPLE_RUN_TIME_S)
      {
          /* You can specify a timeout for the operation and if the a frame is not received
           * within that time limit, pull_frame() returns a nullptr
           *
           * The parameter tells how long time a frame is waited in milliseconds */

          frame = receiver_audio_strm->pull_frame(RECEIVER_WAIT_TIME_MS.count());

          if (frame)
          {
              print_mutex->lock();
              std::cout << "Received an audio frame. Payload size: " << frame->payload_len << std::endl;
              print_mutex->unlock();

              // Process audio frame here

              (void)uvgrtp::frame::dealloc_frame(frame);
          }
      }

      receiver_session->destroy_stream(receiver_audio_strm);
  }
}

void video_receiver(uvgrtp::session* receiver_session, int flags, std::shared_ptr<std::mutex> print_mutex)
{
  /* Keys creates using Diffie-Hellman mode */
  uvgrtp::media_stream *receiver_video_strm =
      receiver_session->create_stream(RECEIVER_VIDEO_PORT, SENDER_VIDEO_PORT,
                                      RTP_FORMAT_H266, flags);

  if (receiver_video_strm)
  {
      uvgrtp::frame::rtp_frame *frame = nullptr;

      std::cout << "Start receiving audio frames for " << EXAMPLE_RUN_TIME_S.count()
                << " seconds" << std::endl;
      auto start = std::chrono::steady_clock::now();

      while (std::chrono::steady_clock::now() - start < EXAMPLE_RUN_TIME_S)
      {
          /* You can specify a timeout for the operation and if the a frame is not received
           * within that time limit, pull_frame() returns a nullptr
           *
           * The parameter tells how long time a frame is waited in milliseconds */

          frame = receiver_video_strm->pull_frame(RECEIVER_WAIT_TIME_MS.count());

          if (frame)
          {
              print_mutex->lock();
              std::cout << "Received a video frame. Payload size: " << frame->payload_len << std::endl;
              print_mutex->unlock();

              // Process video frame here

              (void)uvgrtp::frame::dealloc_frame(frame);
          }
      }

      receiver_session->destroy_stream(receiver_video_strm);
  }
}

void audio_sender(uvgrtp::session* sender_session, int flags, std::shared_ptr<std::mutex> print_mutex)
{
    /* The first call to create_stream() creates keys for the session using Diffie-Hellman
     * key exchange and all subsequent calls to create_stream() initialize keys for the
     * stream using Multistream mode */
    uvgrtp::media_stream *sender_audio_strm = sender_session->create_stream(SENDER_AUDIO_PORT,
                                                                            RECEIVER_AUDIO_PORT,
                                                                            RTP_FORMAT_GENERIC, flags);

    if (sender_audio_strm)
    {
        auto start = std::chrono::steady_clock::now();

        for (int i = 0; std::chrono::steady_clock::now() < (start + EXAMPLE_RUN_TIME_S); ++i)
        {
            print_mutex->lock();
            std::cout << "Sending audio frame" << std::endl;
            print_mutex->unlock();

            std::unique_ptr<uint8_t[]> dummy_audio_frame = std::unique_ptr<uint8_t[]>(new uint8_t[AUDIO_PAYLOAD_SIZE]);

            if (sender_audio_strm->push_frame(std::move(dummy_audio_frame), AUDIO_PAYLOAD_SIZE, RTP_NO_FLAGS) != RTP_OK)
            {
                std::cerr << "Failed to send audio frame" << std::endl;
            }

            // wait until it is time to send the next frame. Included only for
            // demostration purposes since you can use uvgRTP to send packets as fast as desired
            auto time_since_start = std::chrono::steady_clock::now() - start;
            auto next_frame_time = (i + 1)*std::chrono::milliseconds(AUDIO_FRAME_INTERVAL_MS);
            if (next_frame_time > time_since_start)
            {
                std::this_thread::sleep_for(next_frame_time - time_since_start);
            }
        }
    }
}

void video_sender(uvgrtp::session* sender_session, int flags, std::shared_ptr<std::mutex> print_mutex)
{
  /* Initialize ZRTP and negotiate the keys used to encrypt the media */
  uvgrtp::media_stream *sender_video_strm = sender_session->create_stream(SENDER_VIDEO_PORT,
                                                                          RECEIVER_VIDEO_PORT,
                                                                          RTP_FORMAT_H266, flags);

  if (sender_video_strm)
  {
      auto start = std::chrono::steady_clock::now();

      for (int i = 0; std::chrono::steady_clock::now() < (start + EXAMPLE_RUN_TIME_S); ++i)
      {
          print_mutex->lock();
          std::cout << "Sending video frame" << std::endl;
          print_mutex->unlock();

          std::unique_ptr<uint8_t[]> dummy_audio_frame =
              std::unique_ptr<uint8_t[]>(new uint8_t[VIDEO_PAYLOAD_SIZE]);

          if (sender_video_strm->push_frame(std::move(dummy_audio_frame),
                                            VIDEO_PAYLOAD_SIZE, RTP_NO_FLAGS) != RTP_OK)
          {
              std::cerr << "Failed to send video frame" << std::endl;
          }

          // wait until it is time to send the next frame. Included only for
          // demostration purposes since you can use uvgRTP to send packets as fast as desired
          auto time_since_start = std::chrono::steady_clock::now() - start;
          auto next_frame_time = (i + 1)*std::chrono::milliseconds(VIDEO_FRAME_INTERVAL_MS);
          if (next_frame_time > time_since_start)
          {
              std::this_thread::sleep_for(next_frame_time - time_since_start);
          }
      }
  }
}
