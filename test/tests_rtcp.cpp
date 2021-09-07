#include "lib.hh"
#include <gtest/gtest.h>

constexpr char LOCAL_INTERFACE[] = "127.0.0.1";
constexpr uint16_t LOCAL_PORT = 9200;

constexpr char REMOTE_ADDRESS[] = "127.0.0.1";
constexpr uint16_t REMOTE_PORT = 9202;

constexpr uint16_t PAYLOAD_LEN = 256;
constexpr uint16_t FRAME_RATE = 30;
constexpr uint32_t EXAMPLE_RUN_TIME_S = 15;
constexpr int SEND_TEST_PACKETS = FRAME_RATE * EXAMPLE_RUN_TIME_S;
constexpr int PACKET_INTERVAL_MS = 1000 / FRAME_RATE;

void receiver_hook(uvgrtp::frame::rtcp_receiver_report* frame);
void sender_hook(uvgrtp::frame::rtcp_sender_report* frame);

void wait_until_next_frame(std::chrono::steady_clock::time_point& start, int frame_index);
void cleanup(uvgrtp::context& ctx, uvgrtp::session* local_session, uvgrtp::session* remote_session,
    uvgrtp::media_stream* send, uvgrtp::media_stream* receive);



TEST(RTCPTests, rtcp) {
    std::cout << "Starting uvgRTP RTCP unit tests" << std::endl;

    // Creation of RTP stream. See sending example for more details
    uvgrtp::context ctx;
    uvgrtp::session* local_session = ctx.create_session(REMOTE_ADDRESS);
    uvgrtp::session* remote_session = ctx.create_session(LOCAL_INTERFACE);

    int flags = RCE_RTCP;

    uvgrtp::media_stream* local_stream = nullptr;
    if (local_session)
    {
        local_stream = local_session->create_stream(LOCAL_PORT, REMOTE_PORT, RTP_FORMAT_GENERIC, flags);
    }

    uvgrtp::media_stream* remote_stream = nullptr;
    if (remote_session)
    {
        remote_stream = remote_session->create_stream(REMOTE_PORT, LOCAL_PORT, RTP_FORMAT_GENERIC, flags);
    }

    EXPECT_NE(nullptr, local_session);
    EXPECT_NE(nullptr, remote_session);
    EXPECT_NE(nullptr, local_stream);
    EXPECT_NE(nullptr, remote_stream);

    if (local_stream)
    {
        EXPECT_EQ(RTP_OK, local_stream->get_rtcp()->install_receiver_hook(receiver_hook));
    }

    if (remote_stream)
    {
        EXPECT_EQ(RTP_OK, remote_stream->get_rtcp()->install_sender_hook(sender_hook));
    }

    if (local_stream)
    {
        uint8_t buffer[PAYLOAD_LEN] = { 0 };
        memset(buffer, 'a', PAYLOAD_LEN);

        std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();

        for (unsigned int i = 0; i < SEND_TEST_PACKETS; ++i)
        {
            EXPECT_EQ(RTP_OK, local_stream->push_frame((uint8_t*)buffer, PAYLOAD_LEN, RTP_NO_FLAGS));

            wait_until_next_frame(start, i);
        }
    }

    cleanup(ctx, local_session, remote_session, local_stream, remote_stream);
}



void receiver_hook(uvgrtp::frame::rtcp_receiver_report* frame)
{
    std::cout << "RTCP receiver report! ----------" << std::endl;

    for (auto& block : frame->report_blocks)
    {
        std::cout << "ssrc: " << block.ssrc << std::endl;
        std::cout << "fraction: " << block.fraction << std::endl;
        std::cout << "lost: " << block.lost << std::endl;
        std::cout << "last_seq: " << block.last_seq << std::endl;
        std::cout << "jitter: " << block.jitter << std::endl;
        std::cout << "lsr: " << block.lsr << std::endl;
        std::cout << "dlsr (ms): " << uvgrtp::clock::jiffies_to_ms(block.dlsr)
            << std::endl << std::endl;
    }

    /* RTCP frames can be deallocated using delete */
    delete frame;
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
        std::cout << "fraction: " << block.fraction << std::endl;
        std::cout << "lost: " << block.lost << std::endl;
        std::cout << "last_seq: " << block.last_seq << std::endl;
        std::cout << "jitter: " << block.jitter << std::endl;
        std::cout << "lsr: " << block.lsr << std::endl;
        std::cout << "dlsr (ms): " << uvgrtp::clock::jiffies_to_ms(block.dlsr)
            << std::endl << std::endl;
    }

    /* RTCP frames can be deallocated using delete */
    delete frame;
}

void wait_until_next_frame(std::chrono::steady_clock::time_point& start, int frame_index)
{
    // wait until it is time to send the next frame. Simulates a steady sending pace
    // and included only for demostration purposes since you can use uvgRTP to send
    // packets as fast as desired
    auto time_since_start = std::chrono::steady_clock::now() - start;
    auto next_frame_time = (frame_index + 1) * std::chrono::milliseconds(PACKET_INTERVAL_MS);
    if (next_frame_time > time_since_start)
    {
        std::this_thread::sleep_for(next_frame_time - time_since_start);
    }
}

void cleanup(uvgrtp::context& ctx, uvgrtp::session* local_session, uvgrtp::session* remote_session,
    uvgrtp::media_stream* send, uvgrtp::media_stream* receive)
{
    if (send)
    {
        local_session->destroy_stream(send);
    }

    if (receive)
    {
        remote_session->destroy_stream(receive);
    }

    if (local_session)
    {
        // Session must be destroyed manually
        ctx.destroy_session(local_session);
    }

    if (remote_session)
    {
        // Session must be destroyed manually
        ctx.destroy_session(remote_session);
    }
}