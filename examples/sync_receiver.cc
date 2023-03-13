#include <uvgrtp/lib.hh>
#include <climits>
#include <cstring>
#include <vector>
#include <iostream>

/* RTP Control Protocol (RTCP) can be used to synchronize audio and video streams. RTCP Sender Reports (SR)
 *  contain two inportant fields: NTP timestamp and RTP timestamp. These pairs can be used to sync
 *  audio and video streams together. See RFC 3550 5.1 and 6.4.1 for more info
 *
 * This example demonstrates using RTCP to synchronize audio and video streams. There are two
 * receivers representing video and audio streams. This example also measures the latency between
 * generating Sender report and receiving them. For the latency measurements to be accurate, you
 * must make sure that the system clocks are synchronized! However, for only synchronizing audio and video, this
 * is not crucial.
 *
 * This receiver example prints out the NTP and RTP timestamps from RTCP Sender Reports. The process of syncing
 * audio and video streams using these timestamps follows these guidelines but exact implementation is up to the user:
 * 
 * 1. The RTP+NTP pair in the video streams SR allows you to convert video stream RTP timestamps into NTP timestamps.
 *    In other words, it lets you know which RTP and NTP timestamps belong together.
 * 
 * 2. Map this NTP timestamp into an audio streams RTP timestamp using the RTP+NTP timestamp pair from the audio
 *    streams Sender Reports
 * 
 * 3. Now both audio and video streams RTP timestamps are in the audio streams timestamp "format" and can be synchronized
 *
 * This example is intended to be ran with sync_sender example. Start sync_receiver first and then
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

/* Save audio and video ssrcs for telling streams from each other */
uint32_t video_ssrc = 0;
uint32_t audio_ssrc = 0;

void sender_hook(uvgrtp::frame::rtcp_sender_report* frame);
void rtp_receive_hook(void* arg, uvgrtp::frame::rtp_frame* frame);

void receive_function(uvgrtp::session* receiver_session, int flags, std::shared_ptr<std::mutex> print_mutex,
    RTP_FORMAT format, uint16_t receiver_port, uint16_t sender_port);

int main(void)
{
    std::cout << "Starting uvgRTP RTCP stream synchronization example" << std::endl;

    std::shared_ptr<std::mutex> print_mutex = std::shared_ptr<std::mutex>(new std::mutex);
    unsigned rce_dh_flags = RCE_RTCP;
    unsigned rce_multistream_flags = RCE_RTCP;

    std::cout << "Initializing receivers" << std::endl;
    uvgrtp::context receiver_ctx;
    uvgrtp::session* receiver_session = receiver_ctx.create_session(SENDER_ADDRESS, RECEIVER_ADDRESS);

    // start the audio and video receivers in different threads
    std::thread a_receiver(receive_function, receiver_session, rce_dh_flags, print_mutex,
        RTP_FORMAT_OPUS, RECEIVER_AUDIO_PORT, SENDER_AUDIO_PORT);

    std::thread v_receiver(receive_function, receiver_session, rce_multistream_flags, print_mutex,
        RTP_FORMAT_H265, RECEIVER_VIDEO_PORT, SENDER_VIDEO_PORT);

    // wait until all threads have ended
    if (a_receiver.joinable())
    {
        a_receiver.join();
    }
    if (v_receiver.joinable())
    {
        v_receiver.join();
    }

    if (receiver_session)
        receiver_ctx.destroy_session(receiver_session);

    std::cout << "RTCP stream synchronization example finished" << std::endl;
    return EXIT_SUCCESS;
}

void receive_function(uvgrtp::session* receiver_session, int flags,
    std::shared_ptr<std::mutex> print_mutex,
    RTP_FORMAT format, uint16_t receiver_port, uint16_t sender_port)
{

    print_mutex->lock();
    std::cout << "Receiver thread port: " << receiver_port << "<-" << sender_port << std::endl;
    print_mutex->unlock();

    uvgrtp::media_stream* receiver_stream =
        receiver_session->create_stream(receiver_port, sender_port, format, flags);

    if (!receiver_stream || receiver_stream->get_rtcp()->install_sender_hook(sender_hook) != RTP_OK)
    {
        std::cerr << "Failed to install RTCP sender report hook" << std::endl;
    }

    if (!receiver_stream || receiver_stream->install_receive_hook(nullptr, rtp_receive_hook) != RTP_OK)
    {
        std::cerr << "Failed to install RTP reception hook";
        return;
    }
    std::this_thread::sleep_for(EXAMPLE_RUN_TIME_S); // lets this example run for some time
}

void sender_hook(uvgrtp::frame::rtcp_sender_report* frame)
{
    uint64_t current_ntp = uvgrtp::clock::ntp::now();
    uint32_t msw = frame->sender_info.ntp_msw;
    uint32_t lsw = frame->sender_info.ntp_lsw;
    uint64_t ntp_ts = (uint64_t(msw) << 32) | lsw;
    uint64_t diff_ms = uvgrtp::clock::ntp::diff(ntp_ts, current_ntp);

    if (frame->ssrc == video_ssrc) {
        std::cout << "Video stream RTCP sender report! ----------" << std::endl;
    }

    if (frame->ssrc == audio_ssrc) {
        std::cout << "Audio stream RTCP sender report! ----------" << std::endl;
    }

    /* This pair can be used to synchronize the streams */
    std::cout << "---RTP timestamp: " << frame->sender_info.rtp_ts << std::endl;
    std::cout << "---NTP timestamp: " << ntp_ts << std::endl;

    /* Latency between sending and receiving in milliseconds */
    std::cout << "---Difference between SR generation and current NTP time (ms): " << diff_ms << std::endl;

    /* RTCP frames can be deallocated using delete */
    delete frame;
}

void rtp_receive_hook(void* arg, uvgrtp::frame::rtp_frame* frame)
{
    /* Check which ssrcs belong to which streams */
    if (frame->header.payload == RTP_FORMAT_OPUS) {
        if (audio_ssrc == 0) {
            audio_ssrc = frame->header.ssrc;
        }
    }
    else if (frame->header.payload == RTP_FORMAT_H265) {
        if (video_ssrc == 0) {
            video_ssrc = frame->header.ssrc;
        }
    }
    (void)uvgrtp::frame::dealloc_frame(frame);
}
