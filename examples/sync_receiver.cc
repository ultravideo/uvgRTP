#include <uvgrtp/lib.hh>
#include <climits>
#include <cstring>
#include <vector>
#include <fstream>

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


uint32_t latest_audio_rtp_ts = 0;
uint32_t latest_video_rtp_ts = 0;

uint32_t video_ssrc = 0;
uint32_t audio_ssrc = 0;
uint32_t count = 0;
std::vector<int> v_latencies = {};
std::vector<int> v_rtp_diffs = {};

std::vector<int> a_latencies = {};
std::vector<int> a_rtp_diffs = {};



void sender_hook(uvgrtp::frame::rtcp_sender_report* frame);
void receiver_hook(uvgrtp::frame::rtcp_receiver_report* frame);
void rtp_receive_hook(void* arg, uvgrtp::frame::rtp_frame* frame);



void receive_function(uvgrtp::session* receiver_session, int flags, std::shared_ptr<std::mutex> print_mutex,
    RTP_FORMAT format, uint16_t receiver_port, uint16_t sender_port);

int main(void)
{
    std::cout << "Starting uvgRTP SRTP together with ZRTP example" << std::endl;


    uvgrtp::context receiver_ctx;

    // check that Crypto++ has been compiled into uvgRTP, otherwise encryption wont work.
    /*if (!receiver_ctx.crypto_enabled())
    {
        std::cerr << "Cannot run SRTP example if crypto++ is not included in uvgRTP!"
            << std::endl;
        return EXIT_FAILURE;
    }*/

    std::cout << "Initializing receivers" << std::endl;
    uvgrtp::session* receiver_session = receiver_ctx.create_session(SENDER_ADDRESS, RECEIVER_ADDRESS);

    std::shared_ptr<std::mutex> print_mutex = std::shared_ptr<std::mutex>(new std::mutex);

    /* Create separate thread for the receiver
     *
     * Because we're using ZRTP for SRTP key management,
     * the receiver and sender must communicate with each other
     * before the actual media communication starts */

     // Enable SRTP and use ZRTP to manage keys for both sender and receiver*/
    //unsigned rce_dh_flags = RCE_SRTP | RCE_SRTP_KMNGMNT_ZRTP | RCE_ZRTP_DIFFIE_HELLMAN_MODE | RCE_RTCP;
    //unsigned rce_multistream_flags = RCE_SRTP | RCE_SRTP_KMNGMNT_ZRTP | RCE_ZRTP_MULTISTREAM_MODE | RCE_RTCP;

    unsigned rce_dh_flags = RCE_RTCP;
    unsigned rce_multistream_flags = RCE_RTCP;

    // start the receivers in a separate thread
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

    std::cout << "ZRTP example finished" << std::endl;

    std::ofstream file;
    file.open("latency_output.txt");
    file << "Latencies in video stream (ms): Current NTP time - NTP time in SR" << std::endl;
    for (auto i : v_latencies) {
        file << "*  " << i << std::endl;
    }

    file << "RTP timestamp differences in video stream: SR RTP ts - latest RTP packet RTP ts" << std::endl;
    for (auto i : v_rtp_diffs) {
        file << "*  " << i << std::endl;
    }

    file << "Latencies in audio stream (ms): Current NTP time - NTP time in SR" << std::endl;
    for (auto i : a_latencies) {
        file << "*  " << i << std::endl;
    }

    file << "RTP timestamp differences in audio stream: SR RTP ts - latest RTP packet RTP ts" << std::endl;
    for (auto i : a_rtp_diffs) {
        file << "*  " << i << std::endl;
    }
    file.close();

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
    uvgrtp::media_stream* receiver_stream =
        receiver_session->create_stream(receiver_port, sender_port, format, flags);

    if (!receiver_stream || receiver_stream->get_rtcp()->install_receiver_hook(receiver_hook) != RTP_OK)
    {
        std::cerr << "Failed to install RTCP receiver report hook" << std::endl;
    }

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

void receiver_hook(uvgrtp::frame::rtcp_receiver_report* frame)
{
    std::cout << "RTCP receiver report! ----------" << std::endl;

    for (auto& block : frame->report_blocks)
    {
        std::cout << "ssrc: " << block.ssrc << std::endl;
        std::cout << "fraction field value: " << uint32_t(block.fraction) << std::endl;
        std::cout << "fraction: " << float(block.fraction) / 256 << std::endl;
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

void sender_hook(uvgrtp::frame::rtcp_sender_report* frame)
{
    uint64_t current_ntp = uvgrtp::clock::ntp::now();
    uint32_t msw = frame->sender_info.ntp_msw;
    uint32_t lsw = frame->sender_info.ntp_lsw;
    uint64_t ntp_ts = (uint64_t(msw) << 32) | lsw;
    uint64_t diff_ms = uvgrtp::clock::ntp::diff(ntp_ts, current_ntp);
    int diff = -10000; // "impossible" value, when real value is not found

    /*std::cout << "RTCP sender report! ----------" << std::endl;

    std::cout << "---Senders ssrc: " << frame->ssrc << std::endl;
    std::cout << "---NTP msw: " << frame->sender_info.ntp_msw << std::endl;
    std::cout << "---NTP lsw: " << frame->sender_info.ntp_lsw << std::endl;
    std::cout << "---RTP timestamp: " << frame->sender_info.rtp_ts << std::endl;
    std::cout << "---packet count: " << frame->sender_info.pkt_cnt << std::endl;
    std::cout << "---byte count: " << frame->sender_info.byte_cnt << std::endl;*/
    //std::cout << "---Difference between SR generation and current NTP time (ms): " << diff_ms << std::endl;

    if (frame->ssrc == video_ssrc) {
        //std::cout << "---Latest RTP ts: Video: " << latest_video_rtp_ts << std::endl;
        diff = frame->sender_info.rtp_ts - latest_video_rtp_ts;
        //std::cout << "---Video stream: SR RTP ts - latest ts from RTP packet: " << diff << std::endl;
        v_latencies.push_back(diff_ms);
        v_rtp_diffs.push_back(diff);
    }

    if (frame->ssrc == audio_ssrc) {
        //std::cout << "---Latest RTP ts: Audio: " << latest_audio_rtp_ts << std::endl;
        diff = frame->sender_info.rtp_ts - latest_audio_rtp_ts;
        a_latencies.push_back(diff_ms);
        a_rtp_diffs.push_back(diff);
        //std::cout << "----------------------------------------------------Audio stream: SR RTP ts - latest ts from RTP packet: " << diff << std::endl;

    }





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

void rtp_receive_hook(void* arg, uvgrtp::frame::rtp_frame* frame)
{

    if (frame->header.payload == RTP_FORMAT_OPUS) {
        latest_audio_rtp_ts = frame->header.timestamp;
        if (audio_ssrc == 0) {
            audio_ssrc = frame->header.ssrc;
        }
    }
    else if (frame->header.payload == RTP_FORMAT_H265) {
        latest_video_rtp_ts = frame->header.timestamp;
        if (video_ssrc == 0) {
            video_ssrc = frame->header.ssrc;
        }
    }
    (void)uvgrtp::frame::dealloc_frame(frame);
}
