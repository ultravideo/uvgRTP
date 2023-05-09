#include "test_common.hh"

#include <numeric>


constexpr char LOCAL_INTERFACE[] = "::1";
constexpr uint16_t SEND_PORT = 9300;

constexpr char REMOTE_ADDRESS[] = "::1";
constexpr uint16_t RECEIVE_PORT = 9400;

constexpr char MULTICAST_ADDRESS[] = "FF02:0:0:0:0:0:0:0";

// RTCP TEST PARAMETERS
constexpr uint16_t PAYLOAD_LEN = 256;
constexpr uint16_t FRAME_RATE = 30;
constexpr uint32_t EXAMPLE_RUN_TIME_S = 4;
constexpr int SEND_TEST_PACKETS = FRAME_RATE * EXAMPLE_RUN_TIME_S;
constexpr int PACKET_INTERVAL_MS = 1000 / FRAME_RATE;

// FORMATS TEST PARAMETERS
constexpr int AMOUNT_OF_TEST_PACKETS = 100;

// ZRTP TEST PARAMETERS
constexpr auto EXAMPLE_DURATION_S = std::chrono::seconds(2);
// encryption parameters of example
enum Key_length { SRTP_128 = 128, SRTP_196 = 196, SRTP_256 = 256 };
constexpr int SALT_SIZE = 112;
constexpr int SALT_SIZE_BYTES = SALT_SIZE / 8;

void rtp_receive_hook(void* arg, uvgrtp::frame::rtp_frame* frame);
void process_rtp_frame(uvgrtp::frame::rtp_frame* frame);
void test_wait(int time_ms, uvgrtp::media_stream* receiver);

void receiver_hook(uvgrtp::frame::rtcp_receiver_report* frame);
void sender_hook(uvgrtp::frame::rtcp_sender_report* frame);
void sdes_hook(uvgrtp::frame::rtcp_sdes_packet* frame);
void app_hook(uvgrtp::frame::rtcp_app_packet* frame);
void cleanup(uvgrtp::context& ctx, uvgrtp::session* local_session, uvgrtp::session* remote_session,
    uvgrtp::media_stream* send, uvgrtp::media_stream* receive);

void zrtp_sender_func6(uvgrtp::session* sender_session, int sender_port, int receiver_port, unsigned int flags);
void zrtp_receive_func6(uvgrtp::session* receiver_session, int sender_port, int receiver_port, unsigned int flags);

TEST(RTPTests_ip6, rtp_hook_ip6)
{
    // Tests installing a hook to uvgRTP
    std::cout << "Starting IPv6 RTP hook test" << std::endl;
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

TEST(RTPTests_ip6, rtp_copy_ip6)
{
    // Tests installing a hook to uvgRTP
    std::cout << "Starting IPv6 RTP hook test" << std::endl;
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

TEST(RTPTests_ip6, rtp_holepuncher_ip6)
{
    // Tests installing a hook to uvgRTP
    std::cout << "Starting IPv6 RTP holepuncher test" << std::endl;
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

TEST(RTPTests_ip6, rtp_configuration_ip6)
{
    // Tests installing a hook to uvgRTP
    std::cout << "Starting IPv6 RTP configuration test" << std::endl;
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

TEST(RTPTests_ip6, rtp_send_receive_only_flags_ip6)
{
    // Tests installing a hook to uvgRTP
    std::cout << "Starting IPv6 RTP send_receive_only test" << std::endl;
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

TEST(RTCPTests_ip6, rtcp_ip6) {
    std::cout << "Starting uvgRTP IPv6 RTCP tests" << std::endl;

    // Creation of RTP stream. See sending example for more details
    uvgrtp::context ctx;
    uvgrtp::session* local_session = ctx.create_session(REMOTE_ADDRESS, LOCAL_INTERFACE);
    uvgrtp::session* remote_session = ctx.create_session(LOCAL_INTERFACE, REMOTE_ADDRESS);

    int flags = RCE_RTCP;

    uvgrtp::media_stream* local_stream = nullptr;
    if (local_session)
    {
        local_stream = local_session->create_stream(SEND_PORT, RECEIVE_PORT, RTP_FORMAT_GENERIC, flags);
        local_stream->configure_ctx(RCC_SESSION_BANDWIDTH, 3000);
    }

    uvgrtp::media_stream* remote_stream = nullptr;
    if (remote_session)
    {
        remote_stream = remote_session->create_stream(RECEIVE_PORT, SEND_PORT, RTP_FORMAT_GENERIC, flags);
        remote_stream->configure_ctx(RCC_SESSION_BANDWIDTH, 3000);

    }

    EXPECT_NE(nullptr, remote_stream);

    if (local_stream)
    {
        EXPECT_EQ(RTP_OK, local_stream->get_rtcp()->install_receiver_hook(receiver_hook));
        EXPECT_EQ(RTP_OK, local_stream->get_rtcp()->install_sdes_hook(sdes_hook));
    }

    if (remote_stream)
    {
        EXPECT_EQ(RTP_OK, remote_stream->get_rtcp()->install_sender_hook(sender_hook));
        EXPECT_EQ(RTP_OK, remote_stream->get_rtcp()->install_sdes_hook(sdes_hook));
    }

    std::unique_ptr<uint8_t[]> test_frame = std::unique_ptr<uint8_t[]>(new uint8_t[PAYLOAD_LEN]);
    memset(test_frame.get(), 'b', PAYLOAD_LEN);
    send_packets(std::move(test_frame), PAYLOAD_LEN, local_session, local_stream, SEND_TEST_PACKETS, PACKET_INTERVAL_MS, true, RTP_NO_FLAGS);

    cleanup(ctx, local_session, remote_session, local_stream, remote_stream);
}

TEST(FormatTests_ip6, h264_fragmentation_ip6)
{
    std::cout << "Starting IPv6 h264 fragmentation test" << std::endl;
    uvgrtp::context ctx;
    uvgrtp::session* sess = ctx.create_session(LOCAL_INTERFACE);

    uvgrtp::media_stream* sender = nullptr;
    uvgrtp::media_stream* receiver = nullptr;

    if (sess)
    {
        sender = sess->create_stream(SEND_PORT, RECEIVE_PORT, RTP_FORMAT_H264, RCE_NO_FLAGS);
        receiver = sess->create_stream(RECEIVE_PORT, SEND_PORT, RTP_FORMAT_H264, RCE_NO_FLAGS);
    }

    // the default packet limit for RTP is 1458 where 12 bytes are dedicated to RTP header

    std::vector<size_t> test_sizes = std::vector<size_t>(13);
    std::iota(test_sizes.begin(), test_sizes.end(), 1443);
    test_sizes.insert(test_sizes.end(), { 1501,
        1446 * 2 - 1,
        1446 * 2,
        1446 * 2 + 1,
        5000, 7500, 10000, 25000, 50000 });

    int rtp_flags = RTP_NO_FLAGS;
    int nal_type = 5;
    rtp_format_t format = RTP_FORMAT_H264;
    int test_runs = 10;

    for (auto& size : test_sizes)
    {
        std::unique_ptr<uint8_t[]> intra_frame = create_test_packet(format, nal_type, true, size, rtp_flags);
        test_packet_size(std::move(intra_frame), test_runs, size, sess, sender, receiver, rtp_flags);
    }

    cleanup_ms(sess, sender);
    cleanup_ms(sess, receiver);
    cleanup_sess(ctx, sess);
}

TEST(EncryptionTests_ip6, zrtp_ip6)
{
    uvgrtp::context ctx;

    if (!ctx.crypto_enabled())
    {
        std::cout << "Please link crypto to uvgRTP library in order to tests its ZRTP feature!" << std::endl;
        FAIL();
        return;
    }

    uvgrtp::session* sender_session = ctx.create_session(REMOTE_ADDRESS, LOCAL_INTERFACE);
    uvgrtp::session* receiver_session = ctx.create_session(LOCAL_INTERFACE, REMOTE_ADDRESS);

    unsigned zrtp_flags = RCE_SRTP | RCE_SRTP_KMNGMNT_ZRTP;

    std::unique_ptr<std::thread> sender_thread =
        std::unique_ptr<std::thread>(new std::thread(zrtp_sender_func6, sender_session, SEND_PORT, RECEIVE_PORT, zrtp_flags));

    std::unique_ptr<std::thread> receiver_thread =
        std::unique_ptr<std::thread>(new std::thread(zrtp_receive_func6, receiver_session, SEND_PORT, RECEIVE_PORT, zrtp_flags));

    if (sender_thread && sender_thread->joinable())
    {
        sender_thread->join();
    }

    if (receiver_thread && receiver_thread->joinable())
    {
        receiver_thread->join();
    }

    cleanup_sess(ctx, sender_session);
    cleanup_sess(ctx, receiver_session);
}

TEST(EncryptionTests_ip6, zrtp_authenticate_ip6)
{
    uvgrtp::context ctx;

    if (!ctx.crypto_enabled())
    {
        std::cout << "Please link crypto to uvgRTP library in order to tests its ZRTP feature!" << std::endl;
        FAIL();
        return;
    }

    uvgrtp::session* sender_session = ctx.create_session(REMOTE_ADDRESS, LOCAL_INTERFACE);
    uvgrtp::session* receiver_session = ctx.create_session(LOCAL_INTERFACE, REMOTE_ADDRESS);

    unsigned zrtp_flags = RCE_SRTP | RCE_SRTP_KMNGMNT_ZRTP | RCE_SRTP_AUTHENTICATE_RTP;

    std::unique_ptr<std::thread> sender_thread =
        std::unique_ptr<std::thread>(new std::thread(zrtp_sender_func6, sender_session, SEND_PORT, RECEIVE_PORT, zrtp_flags));

    std::unique_ptr<std::thread> receiver_thread =
        std::unique_ptr<std::thread>(new std::thread(zrtp_receive_func6, receiver_session, SEND_PORT, RECEIVE_PORT, zrtp_flags));

    if (sender_thread && sender_thread->joinable())
    {
        sender_thread->join();
    }

    if (receiver_thread && receiver_thread->joinable())
    {
        receiver_thread->join();
    }

    cleanup_sess(ctx, sender_session);
    cleanup_sess(ctx, receiver_session);
}

TEST(EncryptionTests_ip6, zrtp_multistream_ip6)
{
    uvgrtp::context ctx;

    if (!ctx.crypto_enabled())
    {
        std::cout << "Please link crypto to uvgRTP library in order to tests its ZRTP feature!" << std::endl;
        FAIL();
        return;
    }

    /* Enable SRTP and ZRTP */
    unsigned zrtp_dh_flags = RCE_SRTP | RCE_SRTP_KMNGMNT_ZRTP | RCE_ZRTP_DIFFIE_HELLMAN_MODE;

    // only one of the streams should perform DH
    unsigned int zrtp_multistream_flags = RCE_SRTP | RCE_SRTP_KMNGMNT_ZRTP | RCE_ZRTP_MULTISTREAM_MODE;

    uvgrtp::session* sender_session = ctx.create_session(REMOTE_ADDRESS, LOCAL_INTERFACE);
    uvgrtp::session* receiver_session = ctx.create_session(LOCAL_INTERFACE, REMOTE_ADDRESS);

    std::unique_ptr<std::thread> sender_thread1 =
        std::unique_ptr<std::thread>(new std::thread(zrtp_sender_func6, sender_session, SEND_PORT + 2, RECEIVE_PORT + 2, zrtp_dh_flags));

    std::unique_ptr<std::thread> receiver_thread1 =
        std::unique_ptr<std::thread>(new std::thread(zrtp_receive_func6, receiver_session, SEND_PORT + 2, RECEIVE_PORT + 2, zrtp_dh_flags));

    std::unique_ptr<std::thread> sender_thread2 =
        std::unique_ptr<std::thread>(new std::thread(zrtp_sender_func6, sender_session, SEND_PORT + 4, RECEIVE_PORT + 4, zrtp_multistream_flags));

    std::unique_ptr<std::thread> receiver_thread2 =
        std::unique_ptr<std::thread>(new std::thread(zrtp_receive_func6, receiver_session, SEND_PORT + 4, RECEIVE_PORT + 4, zrtp_multistream_flags));

    if (receiver_thread1 && receiver_thread1->joinable())
    {
        receiver_thread1->join();
    }

    if (sender_thread1 && sender_thread1->joinable())
    {
        sender_thread1->join();
    }

    if (sender_thread2 && sender_thread2->joinable())
    {
        sender_thread2->join();
    }

    if (receiver_thread2 && receiver_thread2->joinable())
    {
        receiver_thread2->join();
    }

    cleanup_sess(ctx, sender_session);
    cleanup_sess(ctx, receiver_session);
}

void zrtp_sender_func6(uvgrtp::session* sender_session, int sender_port, int receiver_port, unsigned int flags)
{
    std::cout << "Starting ZRTP sender thread" << std::endl;

    /* See sending.cc for more details about create_stream() */
    uvgrtp::media_stream* send = nullptr;

    if (sender_session)
    {
        send = sender_session->create_stream(sender_port, receiver_port, RTP_FORMAT_GENERIC, flags);
    }

    auto start = std::chrono::steady_clock::now();

    uvgrtp::frame::rtp_frame* frame = nullptr;

    if (send)
    {
        int test_packets = 10;
        size_t packet_size = 1000;
        int packet_interval_ms = EXAMPLE_DURATION_S.count() * 1000 / test_packets;

        std::unique_ptr<uint8_t[]> test_frame = create_test_packet(RTP_FORMAT_GENERIC, 0, false, packet_size, RTP_NO_FLAGS);
        send_packets(std::move(test_frame), packet_size, sender_session, send, test_packets, packet_interval_ms, false, RTP_NO_FLAGS);
    }

    cleanup_ms(sender_session, send);
}

void zrtp_receive_func6(uvgrtp::session* receiver_session, int sender_port, int receiver_port, unsigned int flags)
{
    std::cout << "Starting ZRTP receiver thread" << std::endl;

    /* See sending.cc for more details about create_stream() */
    uvgrtp::media_stream* recv = nullptr;

    if (receiver_session)
    {
        recv = receiver_session->create_stream(receiver_port, sender_port, RTP_FORMAT_GENERIC, flags);
    }

    auto start = std::chrono::steady_clock::now();

    uvgrtp::frame::rtp_frame* frame = nullptr;

    if (recv)
    {
        while (std::chrono::steady_clock::now() - start < EXAMPLE_DURATION_S)
        {
            frame = recv->pull_frame(10);
            if (frame)
            {
                process_rtp_frame(frame);
            }
        }
    }

    cleanup_ms(receiver_session, recv);
}

TEST(RTPTests_ip6, rtp_multicast_ip6)
{
    // Tests installing a hook to uvgRTP
    std::cout << "Starting IPv6 RTP hook test" << std::endl;
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

TEST(RTPTests_ip6, rtp_multicast_multiple_ip6)
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
    for (auto receiver : receivers) cleanup_ms(sess, receiver);
    cleanup_sess(ctx, sess);
}
