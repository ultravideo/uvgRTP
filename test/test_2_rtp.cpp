#include "lib.hh"
#include <gtest/gtest.h>


// parameters for this test. You can change these to suit your network environment
constexpr uint16_t LOCAL_PORT = 9300;

constexpr char REMOTE_ADDRESS[] = "127.0.0.1";
constexpr uint16_t REMOTE_PORT = 9302;

void rtp_receive_hook(void* arg, uvgrtp::frame::rtp_frame* frame);
void cleanup(uvgrtp::context& ctx, uvgrtp::session* sess, uvgrtp::media_stream* ms);
void process_rtp_frame(uvgrtp::frame::rtp_frame* frame);

void test_wait(int time_ms, uvgrtp::media_stream* receiver)
{
    uvgrtp::frame::rtp_frame* frame = nullptr;
    auto start = std::chrono::high_resolution_clock::now();
    frame = receiver->pull_frame(time_ms);
    int actual_difference = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - start).count();

    EXPECT_EQ(RTP_OK, rtp_errno);
    EXPECT_GE(actual_difference, time_ms);
    EXPECT_LE(actual_difference, time_ms + 50); // allow max 50 ms extra

    if (frame)
        process_rtp_frame(frame);
}


class Test_receiver
{
public:
    Test_receiver(int expectedPackets):
        receivedPackets_(0),
        expectedPackets_(expectedPackets)
    {}

    void receive()
    {
        ++receivedPackets_;
    }

    bool gotAll()
    {
        return receivedPackets_ == expectedPackets_;
    }

private:

    int receivedPackets_;
    int expectedPackets_;
};

// TODO: 1) Test only sending, 2) test sending with different configuration, 3) test receiving with different configurations, and 
// 4) test sending and receiving within same test while checking frame size

TEST(RTPTests, send_too_much)
{
    // Tests sending large amounts of data to make sure nothing breaks because of it

    // TODO: Everything should actually be received even in this case but it probably isn't at the moment
    std::cout << "Starting RTP send too much test" << std::endl;
    uvgrtp::context ctx;
    uvgrtp::session* sess = ctx.create_session(REMOTE_ADDRESS);

    uvgrtp::media_stream* sender = nullptr;
    uvgrtp::media_stream* receiver = nullptr;

    int flags = RTP_NO_FLAGS;

    EXPECT_NE(nullptr, sess);
    if (sess)
    {
        sender = sess->create_stream(LOCAL_PORT, REMOTE_PORT, RTP_FORMAT_H265, flags);
        receiver = sess->create_stream(REMOTE_PORT, LOCAL_PORT, RTP_FORMAT_H265, flags);
    }

    EXPECT_NE(nullptr, receiver);
    if (receiver)
    {
        std::cout << "Installing hook" << std::endl;
        EXPECT_EQ(RTP_OK, receiver->install_receive_hook(nullptr, rtp_receive_hook));
    }

    EXPECT_NE(nullptr, sender);
    if (sender)
    {
        std::cout << "Starting to send data" << std::endl;
        for (unsigned int i = 0; i < 10000; ++i)
        {
            const int frame_size = 200000;
            std::unique_ptr<uint8_t[]> dummy_frame = std::unique_ptr<uint8_t[]>(new uint8_t[frame_size]);

            if (sender->push_frame(std::move(dummy_frame), frame_size, RTP_NO_FLAGS) != RTP_OK)
            {
                std::cout << "Failed to send RTP frame!" << std::endl;
            }

            if (i % 1000 == 999)
            {
                std::cout << "Sent " << (i + 1) * 100 / 10000 << " % of data" << std::endl;
            }
        }


        sess->destroy_stream(sender);
        sender = nullptr;
    }

    cleanup(ctx, sess, receiver);
}

TEST(RTPTests, rtp_hook) 
{
    // Tests installing a hook to uvgRTP
    std::cout << "Starting RTP hook test" << std::endl;
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
    std::cout << "Starting RTP poll test" << std::endl;
    uvgrtp::context ctx;
    uvgrtp::session* sess = ctx.create_session(REMOTE_ADDRESS);

    uvgrtp::media_stream* sender = nullptr;
    uvgrtp::media_stream* receiver = nullptr;

    int flags = RTP_NO_FLAGS;

    EXPECT_NE(nullptr, sess);
    if (sess)
    {
        sender = sess->create_stream(LOCAL_PORT, REMOTE_PORT, RTP_FORMAT_H265, flags);
        receiver = sess->create_stream(REMOTE_PORT, LOCAL_PORT, RTP_FORMAT_H265, flags);
    }

    int test_packets = 10;


    EXPECT_NE(nullptr, receiver);
    EXPECT_NE(nullptr, sender);
    if (sender)
    {
        // TODO: This could be prettier by using functions
        const int frame_size = 1500;

        std::cout << "Sending " << test_packets << " test packets" << std::endl;
        for (unsigned int i = 0; i < test_packets; ++i)
        {
            std::unique_ptr<uint8_t[]> dummy_frame = std::unique_ptr<uint8_t[]>(new uint8_t[frame_size]);

            if (sender->push_frame(std::move(dummy_frame), frame_size, RTP_NO_FLAGS) != RTP_OK)
            {
                std::cout << "Failed to send RTP frame!" << std::endl;
            }
        }

        uvgrtp::frame::rtp_frame* received_frame = nullptr;

        auto start = std::chrono::steady_clock::now();
        rtp_errno = RTP_OK;

        std::cout << "Start pulling data" << std::endl;

        int received_packets_no_timeout = 0;
        while (std::chrono::steady_clock::now() - start < std::chrono::seconds(1))
        {
            if (receiver)
            {
                received_frame = receiver->pull_frame(0);
                EXPECT_EQ(RTP_OK, rtp_errno);
            }

            if (received_frame)
            {
                ++received_packets_no_timeout;
                process_rtp_frame(received_frame);
            }
        }

        EXPECT_EQ(received_packets_no_timeout, test_packets);
        int received_packets_timout = 0;

        std::cout << "Sending " << test_packets << " test packets" << std::endl;

        for (unsigned int i = 0; i < test_packets; ++i)
        {
            std::unique_ptr<uint8_t[]> dummy_frame = std::unique_ptr<uint8_t[]>(new uint8_t[frame_size]);

            if (sender->push_frame(std::move(dummy_frame), frame_size, RTP_NO_FLAGS) != RTP_OK)
            {
                std::cout << "Failed to send RTP frame!" << std::endl;
            }
        }

        std::cout << "Start pulling data with 3 ms timout" << std::endl;

        while (std::chrono::steady_clock::now() - start < std::chrono::seconds(2))
        {
            if (receiver)
            {
                received_frame = receiver->pull_frame(3);
                EXPECT_EQ(RTP_OK, rtp_errno);
            }

            if (received_frame)
            {
                ++received_packets_timout;
                process_rtp_frame(received_frame);
            }
        }

        EXPECT_EQ(received_packets_timout, test_packets);

        test_wait(10, receiver);
        test_wait(100, receiver);
        test_wait(500, receiver);

        if (received_frame)
            process_rtp_frame(received_frame);


        sess->destroy_stream(sender);
        sender = nullptr;
    }



    std::cout << "Finished pulling data" << std::endl;

    cleanup(ctx, sess, receiver);
}

void rtp_receive_hook(void* arg, uvgrtp::frame::rtp_frame* frame)
{
    if (arg != nullptr)
    {
        Test_receiver* tester = (Test_receiver*)arg;
        tester->receive();
    }

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
    EXPECT_EQ(2, frame->header.version);
    (void)uvgrtp::frame::dealloc_frame(frame);
}