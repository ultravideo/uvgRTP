#include <uvgrtp/lib.hh>
#include <climits>
#include <cstring>

#include <iostream>
#include <cstring>

/* RTP Control Protocol (RTCP) can be used to synchronize audio and video streams. RTCP Sender Reports (SR)
 *  contain two inportant fields: NTP timestamp and RTP timestamp. These pairs can be used to sync
 *  audio and video streams together. See RFC 3550 6.4.1 for more info
 *
 * This example demonstrates using RTCP to synchronize audio and video streams. There are two
 * receivers representing video and audio streams. This example also measures the latency between
 * generating Sender report and receiving them. For the latency measurements to be accurate, you
 * must make sure that the system clocks are synchronized! However, for only synchronizing audio and
 * video, this is not crucial.
 *
 * This example is intended to be ran with sync_receiver example. Start sync_receiver first and then
 * sync_sender.
 *
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

constexpr auto EXAMPLE_RUN_TIME_S = std::chrono::seconds(55);
constexpr auto AUDIO_FRAME_INTERVAL_MS = std::chrono::milliseconds(20);

constexpr auto VIDEO_FRAME_INTERVAL_MS = std::chrono::milliseconds(1000 / 60); // 60 fps video

/* In this example, RTP timestamps in RTP packets are manually set by the user. Every packet that contains data from the 
 * same frame should be given the same RTP timestamp. This way the frame can be reassembled in the receiving end.
 * Audio and video streams have their own RTP timestamps. Here we assign the initial timestamps. These initial values
 * SHOULD be random, but for the purpose of this demonstration they are given here as constant values.
 * Different streams should start from different values. */
uint32_t a_ts = 400000;
uint32_t v_ts = 200000;

// Hook for receiving RTCP receiver reports
void receiver_hook(uvgrtp::frame::rtcp_receiver_report* frame);

void sender_function(uvgrtp::session* sender_session, int flags, std::shared_ptr<std::mutex> print_mutex,
    RTP_FORMAT format, uint16_t sender_port, uint16_t receiver_port, size_t payload_size,
    std::chrono::milliseconds frame_interval);
void wait_until_next_frame(std::chrono::steady_clock::time_point& start, std::chrono::milliseconds interval, int frame_index);

int main(void)
{
    std::cout << "Starting uvgRTP RTCP stream synchronization example" << std::endl;

    std::shared_ptr<std::mutex> print_mutex = std::shared_ptr<std::mutex>(new std::mutex);
    unsigned rce_dh_flags = RCE_RTCP;
    unsigned rce_multistream_flags = RCE_RTCP;

    std::cout << "Initializing senders" << std::endl;
    uvgrtp::context sender_ctx;
    uvgrtp::session* sender_session = sender_ctx.create_session(RECEIVER_ADDRESS, SENDER_ADDRESS);

    // start the audio and video in their own threads
    std::thread a_sender(sender_function, sender_session, rce_dh_flags, print_mutex,
        RTP_FORMAT_OPUS, SENDER_AUDIO_PORT, RECEIVER_AUDIO_PORT,
        AUDIO_PAYLOAD_SIZE, AUDIO_FRAME_INTERVAL_MS);

    std::thread v_sender(sender_function, sender_session, rce_multistream_flags, print_mutex,
        RTP_FORMAT_H265, SENDER_VIDEO_PORT, RECEIVER_VIDEO_PORT,
        VIDEO_PAYLOAD_SIZE, VIDEO_FRAME_INTERVAL_MS);

    // wait until all threads have ended
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

    std::cout << "RTCP stream synchronization example finished" << std::endl;
    return EXIT_SUCCESS;
}


void sender_function(uvgrtp::session* sender_session, int flags, std::shared_ptr<std::mutex> print_mutex,
    RTP_FORMAT format, uint16_t sender_port, uint16_t receiver_port, size_t payload_size,
    std::chrono::milliseconds frame_interval)
{
    print_mutex->lock();
    std::cout << "Sender thread port: " << sender_port << "->" << receiver_port << std::endl;
    print_mutex->unlock();

    uvgrtp::media_stream* sender_audio_strm = sender_session->create_stream(sender_port,
        receiver_port,
        format, flags);

    if (!sender_audio_strm || sender_audio_strm->get_rtcp()->install_receiver_hook(receiver_hook) != RTP_OK)
    {
        std::cerr << "Failed to install RTCP receiver report hook" << std::endl;
        return;
    }
   
    /* This is the RTP timestamp of the frames. Check which stream we are sending and set the initial timestamp */
    uint32_t ts;
    if (format == RTP_FORMAT_OPUS) {
        ts = a_ts;
    }
    else {
        ts = v_ts;
    }

    if (sender_audio_strm)
    {
        auto start = std::chrono::steady_clock::now();

        for (int i = 0; std::chrono::steady_clock::now() < (start + EXAMPLE_RUN_TIME_S); ++i)
        {

            std::unique_ptr<uint8_t[]> dummy_frame = std::unique_ptr<uint8_t[]>(new uint8_t[payload_size]);

            if (format == RTP_FORMAT_H265 && payload_size >= 5)
            {
                memset(dummy_frame.get(), 'a', payload_size); // data
                memset(dummy_frame.get(), 0, 3);
                memset(dummy_frame.get() + 3, 1, 1);
                memset(dummy_frame.get() + 4, 1, (19 << 1)); // Intra frame NAL type
            }
            /* When sending data with the push_frame() function, note the following parameters:
             * 
             * ts: RTP timestamp defined in RFC 3550 5.1. Several consecutive RTP packets
             * will have equal timestamps if they are (logically) generated at once, e.g., belong to the same video frame.
             * Consecutive RTP packets MAY contain timestamps that are not monotonic if the data is not transmitted
             * in the order it was sampled, as in the case of MPEG interpolated video frames.  (The sequence numbers of
             * the packets as transmitted will still be monotonic.)
             * 
             * s_ts: NTP timestamp of when the frame was sampled. This should also be the same for all packets
             * belonging to the same frame 
             * 
             * When you wish to synchronize multiple streams together, you SHOULD manually give both these parameters. It
             * is possible to let RTP set them automatically, however that is not recommended. */

            uint64_t clock_ntp = uvgrtp::clock::ntp::now();
            if (sender_audio_strm->push_frame(std::move(dummy_frame), payload_size, ts, clock_ntp, RTP_NO_FLAGS) != RTP_OK)
            {
                std::cerr << "Failed to send frame" << std::endl;
            }
            ts += 1;

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
    auto next_frame_time = (frame_index + 1) * interval;
    if (next_frame_time > time_since_start)
    {
        std::this_thread::sleep_for(next_frame_time - time_since_start);
    }
}

void receiver_hook(uvgrtp::frame::rtcp_receiver_report* frame)
{
    std::cout << "RTCP receiver report received from " << frame->ssrc << std::endl;

    /* RTCP frames can be deallocated using delete */
    delete frame;
}