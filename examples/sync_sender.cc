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

constexpr auto EXAMPLE_RUN_TIME_S = std::chrono::seconds(60);
constexpr auto RECEIVER_WAIT_TIME_MS = std::chrono::milliseconds(50);

constexpr auto AUDIO_FRAME_INTERVAL_MS = std::chrono::milliseconds(20);

constexpr auto VIDEO_FRAME_INTERVAL_MS = std::chrono::milliseconds(1000 / 60); // 60 fps video

constexpr auto   END_WAIT = std::chrono::seconds(1200);
// RTP timestamps, audio and video
uint32_t a_ts = 400000;
uint32_t v_ts = 200000;

void receiver_hook(uvgrtp::frame::rtcp_receiver_report* frame);
void sender_hook(uvgrtp::frame::rtcp_sender_report* frame);

void sender_function(uvgrtp::session* sender_session, int flags, std::shared_ptr<std::mutex> print_mutex,
    RTP_FORMAT format, uint16_t sender_port, uint16_t receiver_port, size_t payload_size,
    std::chrono::milliseconds frame_interval);
void wait_until_next_frame(std::chrono::steady_clock::time_point& start, std::chrono::milliseconds interval, int frame_index);

int main(void)
{
    std::cout << "Starting uvgRTP SRTP together with ZRTP example" << std::endl;


    std::shared_ptr<std::mutex> print_mutex = std::shared_ptr<std::mutex>(new std::mutex);


    // Enable SRTP and use ZRTP to manage keys for both sender and receiver*/
    //unsigned rce_dh_flags          = RCE_SRTP | RCE_SRTP_KMNGMNT_ZRTP | RCE_ZRTP_DIFFIE_HELLMAN_MODE | RCE_RTCP;
    //unsigned rce_multistream_flags = RCE_SRTP | RCE_SRTP_KMNGMNT_ZRTP | RCE_ZRTP_MULTISTREAM_MODE | RCE_RTCP;

    unsigned rce_dh_flags = RCE_RTCP;
    unsigned rce_multistream_flags = RCE_RTCP;


    std::cout << "Initializing senders" << std::endl;
    uvgrtp::context sender_ctx;
    uvgrtp::session* sender_session = sender_ctx.create_session(RECEIVER_ADDRESS, SENDER_ADDRESS);

    // start the senders in their own threads
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

    std::cout << "ZRTP example finished" << std::endl;
    std::this_thread::sleep_for(END_WAIT);

    return EXIT_SUCCESS;
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
    uvgrtp::media_stream* sender_audio_strm = sender_session->create_stream(sender_port,
        receiver_port,
        format, flags);

    if (!sender_audio_strm || sender_audio_strm->get_rtcp()->install_sender_hook(sender_hook) != RTP_OK) {
        std::cerr << "Failed to install RTCP sender report hook" << std::endl;
        return;
    }

    if (!sender_audio_strm || sender_audio_strm->get_rtcp()->install_receiver_hook(receiver_hook) != RTP_OK)
    {
        std::cerr << "Failed to install RTCP receiver report hook" << std::endl;
        return;
    }
   
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

            uint64_t clock_ntp = uvgrtp::clock::ntp::now();// -UINT32_MAX / 100;
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

void sender_hook(uvgrtp::frame::rtcp_sender_report* frame)
{
    std::cout << "RTCP sender report! ----------" << std::endl;
    std::cout << "NTP msw: " << frame->sender_info.ntp_msw << std::endl;
    std::cout << "NTP lsw: " << frame->sender_info.ntp_lsw << std::endl;
    std::cout << "RTP timestamp: " << frame->sender_info.rtp_ts << std::endl;
    std::cout << "packet count: " << frame->sender_info.pkt_cnt << std::endl;
    std::cout << "byte count: " << frame->sender_info.byte_cnt << std::endl;

    for (auto& block : frame->report_blocks)
    {
        std::cout << "ssrc: " << block.ssrc << std::endl;
        std::cout << "fraction: " << uint32_t(block.fraction) << std::endl;
        std::cout << "lost: " << block.lost << std::endl;
        std::cout << "last_seq: " << block.last_seq << std::endl;
        std::cout << "jitter: " << block.jitter << std::endl;
        std::cout << "lsr: " << block.lsr << std::endl;
        std::cout << "dlsr (jiffies): " << uvgrtp::clock::jiffies_to_ms(block.dlsr)
            << std::endl << std::endl;
    }

    /* RTCP frames can be deallocated using delete */
    delete frame;
}

void receiver_hook(uvgrtp::frame::rtcp_receiver_report* frame)
{
    /*std::cout << "RTCP receiver report! ----------" << std::endl;
    std::cout << "---Receivers own ssrc: " << frame->ssrc << std::endl;

    for (auto& block : frame->report_blocks)
    {
        std::cout << "---ssrc: " << block.ssrc << std::endl;
        std::cout << "---fraction field value: " << uint32_t(block.fraction) << std::endl;
        std::cout << "---fraction: " << float(block.fraction) / 256 << std::endl;
        std::cout << "---lost: " << block.lost << std::endl;
        std::cout << "---last_seq: " << block.last_seq << std::endl;
        std::cout << "---jitter: " << block.jitter << std::endl;
        std::cout << "---lsr: " << block.lsr << std::endl;
        std::cout << "---dlsr (jiffies): " << uvgrtp::clock::jiffies_to_ms(block.dlsr)
            << std::endl << std::endl;
    }*/

    /* RTCP frames can be deallocated using delete */
    delete frame;
}