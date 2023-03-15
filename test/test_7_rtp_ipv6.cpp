#include "test_common.hh"


/* TODO: 1) Test only sending, 2) test sending with different configuration, 3) test receiving with different configurations, and
 * 4) test sending and receiving within same test while checking frame size */

 // parameters for this test. You can change these to suit your network environment
constexpr uint16_t SEND_PORT = 9300;

constexpr char REMOTE_ADDRESS[] = "::1";
constexpr uint16_t RECEIVE_PORT = 9302;

void rtp_receive_hook(void* arg, uvgrtp::frame::rtp_frame* frame);
void process_rtp_frame(uvgrtp::frame::rtp_frame* frame);

void test_wait(int time_ms, uvgrtp::media_stream* receiver);

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
