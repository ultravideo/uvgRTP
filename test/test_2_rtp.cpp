#include "test_common.hh"
#include <array>

/* TODO: 1) Test only sending, 2) test sending with different configuration, 3) test receiving with different configurations, and 
 * 4) test sending and receiving within same test while checking frame size */

// parameters for this test. You can change these to suit your network environment
constexpr uint16_t SEND_PORT = 9300;

constexpr char REMOTE_ADDRESS[] = "127.0.0.1";
constexpr char MULTICAST_ADDRESS[] = "224.0.0.122";
constexpr uint16_t RECEIVE_PORT = 9302;

void rtp_receive_hook(void* arg, uvgrtp::frame::rtp_frame* frame);
void process_rtp_frame(uvgrtp::frame::rtp_frame* frame);
void user_hook(void* arg, uint8_t* data, uint32_t len);

int recv_;

void user_hook(void* arg, uint8_t* data, uint32_t len)
{
    EXPECT_EQ(len, 5);
    std::array<uint8_t, 5> recv_arr {data[0], data[1], data[2], data[3], data[4] };
    std::array<uint8_t, 5> expected_arr = {20,25,30,35,40};
    EXPECT_EQ(recv_arr, expected_arr);
    recv_++;
    std::cout << "User frame received correctly, size " << len << std::endl;
}

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

    int test_packets = 10;
    std::vector<size_t> sizes = { 1000, 2000 };
    for (size_t& size : sizes)
    {
        std::unique_ptr<uint8_t[]> test_frame = create_test_packet(RTP_FORMAT_GENERIC, 0, false, size, RTP_NO_FLAGS);
        test_packet_size(std::move(test_frame), test_packets, size, sess, sender, receiver, RTP_NO_FLAGS);
    }

    cleanup_ms(sess, sender);
    cleanup_ms(sess, receiver);
    cleanup_sess(ctx, sess);
}

TEST(RTPTests, rtp_copy)
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

    int test_packets = 10;
    std::vector<size_t> sizes = { 1000, 2000 };
    for (size_t& size : sizes)
    {
        int rtp_flags = RTP_COPY;
        std::unique_ptr<uint8_t[]> test_frame = create_test_packet(RTP_FORMAT_GENERIC, 0, false, size, rtp_flags);
        test_packet_size(std::move(test_frame), test_packets, size, sess, sender, receiver, rtp_flags);
    }

    cleanup_ms(sess, sender);
    cleanup_ms(sess, receiver);
    cleanup_sess(ctx, sess);
}

TEST(RTPTests, rtp_holepuncher)
{
    // Tests installing a hook to uvgRTP
    std::cout << "Starting RTP hook test" << std::endl;
    uvgrtp::context ctx;
    uvgrtp::session* sess = ctx.create_session(REMOTE_ADDRESS);

    uvgrtp::media_stream* sender = nullptr;
    uvgrtp::media_stream* receiver = nullptr;

    int flags = RCE_FRAGMENT_GENERIC | RCE_HOLEPUNCH_KEEPALIVE;
    if (sess)
    {
        sender = sess->create_stream(RECEIVE_PORT, SEND_PORT, RTP_FORMAT_GENERIC, flags);
        receiver = sess->create_stream(SEND_PORT, RECEIVE_PORT, RTP_FORMAT_GENERIC, flags);
    }

    int test_packets = 10;
    std::vector<size_t> sizes = { 1000, 2000 };
    for (size_t& size : sizes)
    {
        std::unique_ptr<uint8_t[]> test_frame = create_test_packet(RTP_FORMAT_GENERIC, 0, false, size, RTP_NO_FLAGS);
        test_packet_size(std::move(test_frame), test_packets, size, sess, sender, receiver, RTP_NO_FLAGS);
    }

    cleanup_ms(sess, sender);
    cleanup_ms(sess, receiver);
    cleanup_sess(ctx, sess);
}

TEST(RTPTests, rtp_configuration)
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

    // here we try to break uvgRTP by calling various configure values
    if (sender)
    {
        sender->configure_ctx(RCC_UDP_SND_BUF_SIZE, 40 * 1000 * 1000);
        sender->configure_ctx(RCC_UDP_SND_BUF_SIZE, 2 * 1000 * 1000);

        sender->configure_ctx(RCC_DYN_PAYLOAD_TYPE, 8);

        sender->configure_ctx(RCC_MTU_SIZE, 800);

        sender->configure_ctx(RCC_FPS_NUMERATOR, 100);
        sender->configure_ctx(RCC_FPS_DENOMINATOR, 1);
    }

    if (receiver)
    {
        receiver->configure_ctx(RCC_UDP_RCV_BUF_SIZE, 20 * 1000 * 1000);
        receiver->configure_ctx(RCC_UDP_RCV_BUF_SIZE, 2 * 1000 * 1000);

        receiver->configure_ctx(RCC_RING_BUFFER_SIZE, 20 * 1000 * 1000);
        receiver->configure_ctx(RCC_RING_BUFFER_SIZE, 2 * 1000 * 1000);

        receiver->configure_ctx(RCC_PKT_MAX_DELAY, 200);

        receiver->configure_ctx(RCC_DYN_PAYLOAD_TYPE, 8);
    }

    int test_packets = 10;
    std::vector<size_t> sizes = { 1000, 2000 };
    for (size_t& size : sizes)
    {
        std::unique_ptr<uint8_t[]> test_frame = create_test_packet(RTP_FORMAT_GENERIC, 0, false, size, RTP_NO_FLAGS);
        test_packet_size(std::move(test_frame), test_packets, size, sess, sender, receiver, RTP_NO_FLAGS);
    }

    cleanup_ms(sess, sender);
    cleanup_ms(sess, receiver);
    cleanup_sess(ctx, sess);
}

/*
TEST(RTPTests, rtp_flags)
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

    int test_packets = 10;
    std::vector<size_t> sizes = { 1000, 2000 };

    int rtp_flags = RTP_COPY;
    for (size_t& size : sizes)
    {
        std::unique_ptr<uint8_t[]> test_frame = create_test_packet(RTP_FORMAT_GENERIC, 0, false, size, rtp_flags);
        test_packet_size(std::move(test_frame), test_packets, size, sess, sender, receiver, rtp_flags);
    }

    cleanup_ms(sess, receiver);
    cleanup_sess(ctx, sess);
}
*/

TEST(RTPTests, rtp_send_receive_only_flags)
{
    // Tests installing a hook to uvgRTP
    std::cout << "Starting RTP hook test" << std::endl;
    uvgrtp::context ctx;
    uvgrtp::session* send_sess = ctx.create_session(REMOTE_ADDRESS);
    uvgrtp::session* receive_sess = ctx.create_session(REMOTE_ADDRESS);

    uvgrtp::media_stream* sender = nullptr;
    uvgrtp::media_stream* receiver = nullptr;

    int send_flags = RCE_FRAGMENT_GENERIC | RCE_SEND_ONLY;
    int receive_flags = RCE_FRAGMENT_GENERIC | RCE_RECEIVE_ONLY;
    if (send_sess)
    {
        sender = send_sess->create_stream(RECEIVE_PORT, SEND_PORT, RTP_FORMAT_GENERIC, send_flags);
    }

    if (receive_sess)
    {
        receiver = receive_sess->create_stream(SEND_PORT, RECEIVE_PORT, RTP_FORMAT_GENERIC, receive_flags);
    }

    int test_packets = 10;
    std::vector<size_t> sizes = { 1000, 2000 };
    for (size_t& size : sizes)
    {
        std::unique_ptr<uint8_t[]> test_frame = create_test_packet(RTP_FORMAT_GENERIC, 0, false, size, RTP_NO_FLAGS);
        test_packet_size(std::move(test_frame), test_packets, size, send_sess, sender, receiver, RTP_NO_FLAGS);
    }

    cleanup_ms(send_sess, sender);
    cleanup_ms(receive_sess, receiver);
    cleanup_sess(ctx, send_sess);
    cleanup_sess(ctx, receive_sess);
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
        std::unique_ptr<uint8_t[]> test_frame = std::unique_ptr<uint8_t[]>(new uint8_t[frame_size]);
        memset(test_frame.get(), 'b', frame_size);
        send_packets(std::move(test_frame), frame_size, sess, sender, test_packets, 0, true, RTP_NO_FLAGS);

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

        test_frame = std::unique_ptr<uint8_t[]>(new uint8_t[frame_size]);
        memset(test_frame.get(), 'b', frame_size);
        send_packets(std::move(test_frame), frame_size, sess, sender, test_packets, 0, true, RTP_NO_FLAGS);

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

TEST(RTPTests, send_large_amounts)
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
    size_t frame_size = 1000;
    int test_packets  = 10000;
    std::unique_ptr<uint8_t[]> test_frame = std::unique_ptr<uint8_t[]>(new uint8_t[frame_size]);
    memset(test_frame.get(), 'b', frame_size);
    send_packets(std::move(test_frame), frame_size, sess, sender, test_packets, 0, true, RTP_NO_FLAGS);

    // send 2000 large packets
    frame_size   = 20000;
    test_packets = 2000;
    test_frame = std::unique_ptr<uint8_t[]>(new uint8_t[frame_size]);
    memset(test_frame.get(), 'b', frame_size);
    send_packets(std::move(test_frame), frame_size, sess, sender, test_packets, 0, true, RTP_NO_FLAGS);

    cleanup_ms(sess, sender);
    cleanup_ms(sess, receiver);
    cleanup_sess(ctx, sess);
}

TEST(RTPTests, rtp_multicast)
{
    // Tests with a multicast address
    std::cout << "Starting RTP multicast test" << std::endl;
    uvgrtp::context ctx;
    uvgrtp::session* sess = ctx.create_session(MULTICAST_ADDRESS, MULTICAST_ADDRESS);

    uvgrtp::media_stream* sender = nullptr;
    uvgrtp::media_stream* receiver = nullptr;

    int flags = RCE_FRAGMENT_GENERIC;
    if (sess)
    {
        sender = sess->create_stream(RECEIVE_PORT, SEND_PORT, RTP_FORMAT_GENERIC, flags);
        receiver = sess->create_stream(SEND_PORT, RECEIVE_PORT, RTP_FORMAT_GENERIC, flags);
    }

    int test_packets = 10;
    std::vector<size_t> sizes = { 1000, 2000 };
    for (size_t& size : sizes)
    {
        std::unique_ptr<uint8_t[]> test_frame = create_test_packet(RTP_FORMAT_GENERIC, 0, false, size, RTP_NO_FLAGS);
        test_packet_size(std::move(test_frame), test_packets, size, sess, sender, receiver, RTP_NO_FLAGS);
    }

    cleanup_ms(sess, sender);
    cleanup_ms(sess, receiver);
    cleanup_sess(ctx, sess);
}

TEST(RTPTests, rtp_multicast_multiple)
{
    // Tests with a multicast address
    std::cout << "Starting RTP multicast test" << std::endl;
    uvgrtp::context ctx;
    uvgrtp::session* sess = ctx.create_session(MULTICAST_ADDRESS, MULTICAST_ADDRESS);

    EXPECT_NE(nullptr, sess);
    if (!sess) return;

    int flags = RCE_FRAGMENT_GENERIC;

    auto sender = sess->create_stream(RECEIVE_PORT, SEND_PORT, RTP_FORMAT_GENERIC, flags);
    std::vector<uvgrtp::media_stream*> receivers = {
        sess->create_stream(SEND_PORT, RECEIVE_PORT, RTP_FORMAT_GENERIC, flags),
        sess->create_stream(SEND_PORT, RECEIVE_PORT, RTP_FORMAT_GENERIC, flags)
    };

    int test_packets = 10;
    std::vector<size_t> sizes = { 1000, 2000 };
    for (size_t& size : sizes)
    {
        std::unique_ptr<uint8_t[]> test_frame = create_test_packet(RTP_FORMAT_GENERIC, 0, false, size, RTP_NO_FLAGS);
        test_packet_size(std::move(test_frame), test_packets, size, sess, sender, receivers, RTP_NO_FLAGS);
    }

    cleanup_ms(sess, sender);
    for (auto receiver: receivers) cleanup_ms(sess, receiver);
    cleanup_sess(ctx, sess);
}

TEST(RTPTests, rtp_multiplex)
{
    // Test multiplexing two RTP streams into a single socket
    std::cout << "Starting RTP multiplexing test" << std::endl;
    uvgrtp::context ctx;
    uvgrtp::session* receiver_sess = ctx.create_session(REMOTE_ADDRESS);
    uvgrtp::session* sender_sess = ctx.create_session(REMOTE_ADDRESS);

    uvgrtp::media_stream* sender1 = nullptr;
    uvgrtp::media_stream* receiver1 = nullptr;
    uvgrtp::media_stream* sender2 = nullptr;
    uvgrtp::media_stream* receiver2 = nullptr;

    int flags = RCE_FRAGMENT_GENERIC;
    if (sender_sess)
    {
        sender1 = sender_sess->create_stream(RECEIVE_PORT, SEND_PORT, RTP_FORMAT_GENERIC, flags);
        sender1->configure_ctx(RCC_SSRC, 11);
        sender1->configure_ctx(RCC_REMOTE_SSRC, 22);
        sender2 = sender_sess->create_stream(RECEIVE_PORT, SEND_PORT, RTP_FORMAT_GENERIC, flags);
        sender2->configure_ctx(RCC_SSRC, 33);
        sender2->configure_ctx(RCC_REMOTE_SSRC, 44);
    }
    if (receiver_sess)
    {
        receiver1 = receiver_sess->create_stream(SEND_PORT, RECEIVE_PORT, RTP_FORMAT_GENERIC, flags);
        receiver1->configure_ctx(RCC_SSRC, 22);
        receiver1->configure_ctx(RCC_REMOTE_SSRC, 11);
        receiver2 = receiver_sess->create_stream(SEND_PORT, RECEIVE_PORT, RTP_FORMAT_GENERIC, flags);
        receiver2->configure_ctx(RCC_SSRC, 44);
        receiver2->configure_ctx(RCC_REMOTE_SSRC, 33);
    }

    int test_packets = 10;
    std::vector<size_t> sizes = { 1000, 2000 };
    for (size_t& size : sizes)
    {
        std::unique_ptr<uint8_t[]> test_frame1 = create_test_packet(RTP_FORMAT_GENERIC, 0, false, size, RTP_NO_FLAGS);
        std::unique_ptr<uint8_t[]> test_frame2 = create_test_packet(RTP_FORMAT_GENERIC, 0, false, size, RTP_NO_FLAGS);
        test_packet_size(std::move(test_frame1), test_packets, size, sender_sess, sender1, receiver1, RTP_NO_FLAGS);
        test_packet_size(std::move(test_frame2), test_packets, size, sender_sess, sender2, receiver2, RTP_NO_FLAGS);
    }

    cleanup_ms(sender_sess, sender1);
    cleanup_ms(sender_sess, sender2);
    cleanup_ms(receiver_sess, receiver1);
    cleanup_ms(receiver_sess, receiver2);
    cleanup_sess(ctx, sender_sess);
    cleanup_sess(ctx, receiver_sess);
}

TEST(RTPTests, rtp_multiplex_poll)
{
    std::cout << "Starting RTP multiplexing via pull_frame test" << std::endl;
    uvgrtp::context ctx;
    uvgrtp::session* receiver_sess = ctx.create_session(REMOTE_ADDRESS);
    uvgrtp::session* sender_sess = ctx.create_session(REMOTE_ADDRESS);

    uvgrtp::media_stream* sender1 = nullptr;
    uvgrtp::media_stream* receiver1 = nullptr;
    uvgrtp::media_stream* sender2 = nullptr;
    uvgrtp::media_stream* receiver2 = nullptr;

    int flags = RCE_FRAGMENT_GENERIC;
    if (sender_sess)
    {
        sender1 = sender_sess->create_stream(RECEIVE_PORT, SEND_PORT, RTP_FORMAT_GENERIC, flags);
        sender1->configure_ctx(RCC_SSRC, 11);
        sender2 = sender_sess->create_stream(RECEIVE_PORT, SEND_PORT, RTP_FORMAT_GENERIC, flags);
        sender2->configure_ctx(RCC_SSRC, 22);
    }
    if (receiver_sess)
    {
        receiver1 = receiver_sess->create_stream(SEND_PORT, RECEIVE_PORT, RTP_FORMAT_GENERIC, flags);
        receiver1->configure_ctx(RCC_REMOTE_SSRC, 11);
        receiver2 = receiver_sess->create_stream(SEND_PORT, RECEIVE_PORT, RTP_FORMAT_GENERIC, flags);
        receiver2->configure_ctx(RCC_REMOTE_SSRC, 22);
    }

    int test_packets = 10;

    if (sender1 && sender2)
    {
        const int frame_size = 1500;
        std::unique_ptr<uint8_t[]> test_frame1 = std::unique_ptr<uint8_t[]>(new uint8_t[frame_size]);
        std::unique_ptr<uint8_t[]> test_frame2 = std::unique_ptr<uint8_t[]>(new uint8_t[frame_size]);
        memset(test_frame1.get(), 'b', frame_size);
        memset(test_frame2.get(), 'b', frame_size);
        send_packets(std::move(test_frame1), frame_size, sender_sess, sender1, test_packets, 0, true, RTP_NO_FLAGS);
        send_packets(std::move(test_frame2), frame_size, sender_sess, sender2, test_packets, 0, true, RTP_NO_FLAGS);

        uvgrtp::frame::rtp_frame* received_frame1 = nullptr;
        uvgrtp::frame::rtp_frame* received_frame2 = nullptr;

        auto start = std::chrono::steady_clock::now();
        rtp_errno = RTP_OK;

        std::cout << "Start pulling data in both streams" << std::endl;

        int received_packets_no_timeout1 = 0;
        int received_packets_no_timeout2 = 0;
        while (std::chrono::steady_clock::now() - start < std::chrono::seconds(1))
        {
            if (receiver1)
            {
                received_frame1 = receiver1->pull_frame(0);
                EXPECT_EQ(RTP_OK, rtp_errno);
            }
            if (receiver2)
            {
                received_frame2 = receiver2->pull_frame(0);
                EXPECT_EQ(RTP_OK, rtp_errno);
            }
            if (received_frame1)
            {
                ++received_packets_no_timeout1;
                process_rtp_frame(received_frame1);
            }
            if (received_frame2)
            {
                ++received_packets_no_timeout2;
                process_rtp_frame(received_frame2);
            }
        }

        EXPECT_EQ(received_packets_no_timeout1, test_packets);
        EXPECT_EQ(received_packets_no_timeout2, test_packets);
        int received_packets_timout1 = 0;
        int received_packets_timout2 = 0;

        test_frame1 = std::unique_ptr<uint8_t[]>(new uint8_t[frame_size]);
        test_frame2 = std::unique_ptr<uint8_t[]>(new uint8_t[frame_size]);
        memset(test_frame1.get(), 'b', frame_size);
        memset(test_frame2.get(), 'b', frame_size);
        send_packets(std::move(test_frame1), frame_size, sender_sess, sender1, test_packets, 0, true, RTP_NO_FLAGS);
        send_packets(std::move(test_frame2), frame_size, sender_sess, sender2, test_packets, 0, true, RTP_NO_FLAGS);

        std::cout << "Start pulling data in both streams with 3 ms timout" << std::endl;

        while (std::chrono::steady_clock::now() - start < std::chrono::seconds(2))
        {
            if (receiver1)
            {
                received_frame1 = receiver1->pull_frame(3);
                EXPECT_EQ(RTP_OK, rtp_errno);
            }
            if (receiver2)
            {
                received_frame2 = receiver2->pull_frame(3);
                EXPECT_EQ(RTP_OK, rtp_errno);
            }

            if (received_frame1)
            {
                ++received_packets_timout1;
                process_rtp_frame(received_frame1);
            }
            if (received_frame2)
            {
                ++received_packets_timout2;
                process_rtp_frame(received_frame2);
            }
        }

        EXPECT_EQ(received_packets_timout1, test_packets);
        EXPECT_EQ(received_packets_timout2, test_packets);

        test_wait(10, receiver1);
        test_wait(100, receiver1);
        test_wait(500, receiver1);
        test_wait(10, receiver2);
        test_wait(100, receiver2);
        test_wait(500, receiver2);

        if (received_frame1) {
            process_rtp_frame(received_frame1);
        }
        if (received_frame2) {
            process_rtp_frame(received_frame2);
        }

    }

    std::cout << "Finished pulling data" << std::endl;

    cleanup_ms(sender_sess, sender1);
    cleanup_ms(sender_sess, sender2);
    cleanup_ms(receiver_sess, receiver1);
    cleanup_ms(receiver_sess, receiver2);
    cleanup_sess(ctx, sender_sess);
    cleanup_sess(ctx, receiver_sess);
}
/* User packets disabled for now
TEST(RTPTests, uvgrtp_user_frames)
{
    // Tests sending and receiving custom UDP packets
    std::cout << "Starting uvgRTP user frames test: Sending custom UDP packets" << std::endl;
    uvgrtp::context ctx;
    uvgrtp::session* sess = ctx.create_session(REMOTE_ADDRESS);

    recv_ = 0;

    uvgrtp::media_stream* sender = nullptr;
    uvgrtp::media_stream* receiver = nullptr;

    int flags = RCE_FRAGMENT_GENERIC;
    if (sess)
    {
        sender = sess->create_stream(RECEIVE_PORT, SEND_PORT, RTP_FORMAT_GENERIC, flags);
        receiver = sess->create_stream(SEND_PORT, RECEIVE_PORT, RTP_FORMAT_GENERIC, flags);
        EXPECT_EQ(RTP_OK, receiver->install_receive_hook(nullptr, rtp_receive_hook));
        EXPECT_EQ(RTP_OK, receiver->install_user_hook(nullptr, user_hook));
    }

    int test_packets = 10;
    std::vector<size_t> sizes = { 1000, 2000 };
    for (size_t& size : sizes)
    {
        std::unique_ptr<uint8_t[]> test_frame = create_test_packet(RTP_FORMAT_GENERIC, 0, false, size, RTP_NO_FLAGS);
        send_packets(std::move(test_frame), size, sess, sender, 10, 30, true, RTP_NO_FLAGS, false, true);

    }
    EXPECT_TRUE(recv_ > 0);
    std::cout << "Received " << recv_ << " user packets" << std::endl;
    cleanup_ms(sess, sender);
    cleanup_ms(sess, receiver);
    cleanup_sess(ctx, sess);
}*/