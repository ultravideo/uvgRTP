#include "lib.hh"
#include <gtest/gtest.h>


// parameters for this test. You can change these to suit your network environment
constexpr uint16_t LOCAL_PORT = 9100;

constexpr char REMOTE_ADDRESS[] = "127.0.0.1";
constexpr uint16_t REMOTE_PORT = 9102;

void rtp_receive_hook(void* arg, uvgrtp::frame::rtp_frame* frame);
void cleanup(uvgrtp::context& ctx, uvgrtp::session* sess, uvgrtp::media_stream* ms);
void process_rtp_frame(uvgrtp::frame::rtp_frame* frame);

// TODO: 1) Test only sending, 2) test sending with different configuration, 3) test receiving with different configurations, and 
// 4) test sending and receiving within same test while checking frame size


TEST(RTPTests, rtp_hook) 
{
    std::cout << "Starting RTP hook unit test" << std::endl;
    uvgrtp::context ctx;
    uvgrtp::session* sess = ctx.create_session(REMOTE_ADDRESS);
    
    uvgrtp::media_stream* receiver = nullptr;

    int flags = RTP_NO_FLAGS;
    if (sess)
    {
        receiver = sess->create_stream(LOCAL_PORT, REMOTE_PORT, RTP_FORMAT_H265, flags);
    }
    
    if (receiver)
    {
        std::cout << "Installing hook" << std::endl;
        EXPECT_EQ(RTP_OK, receiver->install_receive_hook(nullptr, rtp_receive_hook));
    }

    EXPECT_NE(nullptr, sess);
    EXPECT_NE(nullptr, receiver);

    cleanup(ctx, sess, receiver);
}

TEST(RTPTests, rtp_poll)
{
    std::cout << "Starting RTP poll unit test" << std::endl;

    uvgrtp::context ctx;
    uvgrtp::session* sess = ctx.create_session(REMOTE_ADDRESS);
    uvgrtp::media_stream* receiver = nullptr;

    int flags = RCE_NO_FLAGS;
    if (sess)
    {
        receiver = sess->create_stream(LOCAL_PORT, REMOTE_PORT, RTP_FORMAT_H265, flags);
    }
    
    uvgrtp::frame::rtp_frame* frame = nullptr;

    auto start = std::chrono::steady_clock::now();
    rtp_errno = RTP_OK;

    EXPECT_NE(nullptr, sess);
    EXPECT_NE(nullptr, receiver);

    std::cout << "Start pulling data" << std::endl;

    while (std::chrono::steady_clock::now() - start < std::chrono::seconds(1))
    {
        
        frame = receiver->pull_frame(3);

        EXPECT_EQ(RTP_OK, rtp_errno);

        if (frame)
            process_rtp_frame(frame);
    }

    std::cout << "Finished pulling data" << std::endl;

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