#include "test_common.hh"


// TODO: 1) Test only sending, 2) test sending with different configuration, 3) test receiving with different configurations, and 
// 4) test sending and receiving within same test while checking frame size

// parameters for this test. You can change these to suit your network environment
constexpr uint16_t SEND_PORT = 9300;

constexpr char REMOTE_ADDRESS[] = "127.0.0.1";
constexpr uint16_t RECEIVE_PORT = 9302;

void rtp_receive_hook(void* arg, uvgrtp::frame::rtp_frame* frame);
void process_rtp_frame(uvgrtp::frame::rtp_frame* frame);

void test_wait(int time_ms, uvgrtp::media_stream* receiver)
{
    EXPECT_NE(receiver, nullptr);
    if (receiver)
    {
        uvgrtp::frame::rtp_frame* frame = nullptr;
        auto start = std::chrono::high_resolution_clock::now();
        frame = receiver->pull_frame(time_ms);
        int actual_difference =
            std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - start).count();

        EXPECT_EQ(RTP_OK, rtp_errno);
        EXPECT_GE(actual_difference, time_ms);
        EXPECT_LE(actual_difference, time_ms + 50); // allow max 50 ms extra

        if (frame)
            process_rtp_frame(frame);
    }
}

TEST(RTPTests, rtp_hook)
{
    // Tests installing a hook to uvgRTP
    std::cout << "Starting RTP hook test" << std::endl;
    uvgrtp::context ctx;
    uvgrtp::session* sess = ctx.create_session(REMOTE_ADDRESS);

    uvgrtp::media_stream* sender = nullptr;
    uvgrtp::media_stream* receiver = nullptr;

    int flags = RCE_FRAGMENT_GENERIC;
    if (sess)
    {
        sender = sess->create_stream(RECEIVE_PORT, SEND_PORT, RTP_FORMAT_GENERIC, flags);
        receiver = sess->create_stream(SEND_PORT, RECEIVE_PORT, RTP_FORMAT_GENERIC, flags);
    }

    test_packet_size(1000, sess, sender, receiver);
    test_packet_size(2000, sess, sender, receiver);

    cleanup_ms(sess, receiver);
    cleanup_sess(ctx, sess);
}

TEST(RTPTests, rtp_send_test)
{
    // Tests installing a hook to uvgRTP
    std::cout << "Starting RTP hook test" << std::endl;
    uvgrtp::context ctx;
    uvgrtp::session* sess = ctx.create_session(REMOTE_ADDRESS);

    uvgrtp::media_stream* sender = nullptr;
    uvgrtp::media_stream* receiver = nullptr;

    int flags = RCE_FRAGMENT_GENERIC;
    if (sess)
    {
        sender = sess->create_stream(RECEIVE_PORT, SEND_PORT, RTP_FORMAT_GENERIC, flags);
        receiver = sess->create_stream(SEND_PORT, RECEIVE_PORT, RTP_FORMAT_GENERIC, flags);
    }

    test_packet_size(1500, sess, sender, receiver);

    cleanup_ms(sess, sender);
    cleanup_sess(ctx, sess);
}



TEST(RTPTests, rtp_poll)
{
    std::cout << "Starting RTP poll test" << std::endl;
    uvgrtp::context ctx;
    uvgrtp::session* sess = ctx.create_session(REMOTE_ADDRESS);

    uvgrtp::media_stream* sender = nullptr;
    uvgrtp::media_stream* receiver = nullptr;

    int flags = RCE_FRAGMENT_GENERIC;

    EXPECT_NE(nullptr, sess);
    if (sess)
    {
        sender = sess->create_stream(RECEIVE_PORT, SEND_PORT, RTP_FORMAT_GENERIC, flags);
        receiver = sess->create_stream(SEND_PORT, RECEIVE_PORT, RTP_FORMAT_GENERIC, flags);
    }

    int test_packets = 10;

    EXPECT_NE(nullptr, receiver);
    EXPECT_NE(nullptr, sender);
    if (sender)
    {
        const int frame_size = 1500;
        send_packets(sess, sender, test_packets, frame_size, 0, true, false);

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

        send_packets(sess, sender, test_packets, frame_size, 0, true, false);

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
    }

    std::cout << "Finished pulling data" << std::endl;

    cleanup_ms(sess, sender);
    cleanup_ms(sess, receiver);
    cleanup_sess(ctx, sess);
}

TEST(RTPTests, send_too_much)
{
    // Tests sending large amounts of data to make sure nothing breaks because of it

    // TODO: Everything should actually be received even in this case but it probably isn't at the moment
    std::cout << "Starting RTP send too much test" << std::endl;
    uvgrtp::context ctx;
    uvgrtp::session* sess = ctx.create_session(REMOTE_ADDRESS);

    uvgrtp::media_stream* sender = nullptr;
    uvgrtp::media_stream* receiver = nullptr;

    int flags = RCE_FRAGMENT_GENERIC;

    EXPECT_NE(nullptr, sess);
    if (sess)
    {
        sender = sess->create_stream(RECEIVE_PORT, SEND_PORT, RTP_FORMAT_GENERIC, flags);
        receiver = sess->create_stream(SEND_PORT, RECEIVE_PORT, RTP_FORMAT_GENERIC, flags);
    }

    add_hook(nullptr, receiver, rtp_receive_hook);

    // send 10000 small packets
    send_packets(sess, sender, 10000, 1000, 0, true, true);

    // send 2000 large packets
    send_packets(sess, sender, 2000, 20000, 2, true, true);

    cleanup_ms(sess, sender);
    cleanup_ms(sess, receiver);
    cleanup_sess(ctx, sess);
}