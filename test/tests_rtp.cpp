#include "lib.hh"
#include <gtest/gtest.h>


// parameters for this test. You can change these to suit your network environment
constexpr uint16_t LOCAL_PORT = 8890;

constexpr char REMOTE_ADDRESS[] = "127.0.0.1";
constexpr uint16_t REMOTE_PORT = 8888;

void rtp_receive_hook(void* arg, uvgrtp::frame::rtp_frame* frame);
void cleanup(uvgrtp::context& ctx, uvgrtp::session* sess, uvgrtp::media_stream* ms);
void process_rtp_frame(uvgrtp::frame::rtp_frame* frame);

// TODO: 1) Test only sending, 2) test sending with different configuration, 3) test receiving with different configurations, and 
// 4) test sending and receiving within same test while checking frame size


TEST(RTPTests, rtp_hook) 
{
    uvgrtp::context ctx;
    uvgrtp::session* sess = ctx.create_session(REMOTE_ADDRESS);
    EXPECT_NE(nullptr, sess);

    int flags = RTP_NO_FLAGS;
    uvgrtp::media_stream* receiver = sess->create_stream(LOCAL_PORT, REMOTE_PORT, RTP_FORMAT_H265, flags);
    EXPECT_NE(nullptr, receiver);
    EXPECT_EQ(RTP_OK, receiver->install_receive_hook(nullptr, rtp_receive_hook));

    std::this_thread::sleep_for(std::chrono::seconds(1)); // lets this example run for some time

    cleanup(ctx, sess, receiver);
}

TEST(RTPTests, rtp_poll)
{
    uvgrtp::context ctx;
    uvgrtp::session* sess = ctx.create_session(REMOTE_ADDRESS);
    EXPECT_NE(nullptr, sess);

    int flags = RCE_NO_FLAGS;
    uvgrtp::media_stream* receiver = sess->create_stream(LOCAL_PORT, REMOTE_PORT, RTP_FORMAT_H265, flags);
    EXPECT_NE(nullptr, receiver);

    uvgrtp::frame::rtp_frame* frame = nullptr;

    auto start = std::chrono::steady_clock::now();
    rtp_errno = RTP_OK;

    while (std::chrono::steady_clock::now() - start < std::chrono::seconds(1))
    {
        frame = receiver->pull_frame(3);

        EXPECT_EQ(RTP_OK, rtp_errno);

        if (frame)
            process_rtp_frame(frame);
    }

    sess->destroy_stream(receiver);
    

    cleanup(ctx, sess, receiver);
}



void rtp_receive_hook(void* arg, uvgrtp::frame::rtp_frame* frame)
{
    process_rtp_frame(frame);
}

void cleanup(uvgrtp::context& ctx, uvgrtp::session* sess, uvgrtp::media_stream* ms)
{
    if (ms)
    {
        sess->destroy_stream(ms);
    }

    if (sess)
    {
        ctx.destroy_session(sess);
    }
}


void process_rtp_frame(uvgrtp::frame::rtp_frame* frame)
{
    EXPECT_NE(0, frame->payload_len);
    (void)uvgrtp::frame::dealloc_frame(frame);
}