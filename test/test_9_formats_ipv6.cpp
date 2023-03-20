#include "test_common.hh"

#include <numeric>

constexpr uint16_t SEND_PORT = 9100;
constexpr char LOCAL_ADDRESS[] = "::1";
constexpr uint16_t RECEIVE_PORT = 9102;

constexpr int AMOUNT_OF_TEST_PACKETS = 100;
constexpr size_t PAYLOAD_LEN = 100;

// TODO: Use real files

TEST(FormatTests_ip6, h26x_flags_ip6)
{
    std::cout << "Starting IPv6 h26x flag test" << std::endl;

    uvgrtp::context ctx;
    uvgrtp::session* sess = ctx.create_session(LOCAL_ADDRESS);

    uvgrtp::media_stream* sender = nullptr;
    uvgrtp::media_stream* receiver = nullptr;

    int sender_rce_flags = RCE_NO_FLAGS;
    int receiver_rce_flags = RCE_H26X_DO_NOT_PREPEND_SC;

    if (sess)
    {
        sender = sess->create_stream(SEND_PORT, RECEIVE_PORT, RTP_FORMAT_H265, sender_rce_flags);
        receiver = sess->create_stream(RECEIVE_PORT, SEND_PORT, RTP_FORMAT_H265, receiver_rce_flags);
    }

    std::vector<size_t> test_sizes = { 1443, 1501 };
    int rtp_flags = RTP_NO_FLAGS;
    int nal_type = 5;
    rtp_format_t format = RTP_FORMAT_H264;
    int test_runs = 10;

    for (auto& size : test_sizes)
    {
        std::unique_ptr<uint8_t[]> intra_frame = create_test_packet(format, nal_type, true, size, rtp_flags);
        test_packet_size(std::move(intra_frame), test_runs, size, sess, sender, receiver, rtp_flags);
    }

    rtp_flags = RTP_NO_H26X_SCL;
    for (auto& size : test_sizes)
    {
        std::unique_ptr<uint8_t[]> intra_frame = create_test_packet(format, nal_type, true, size, rtp_flags);
        test_packet_size(std::move(intra_frame), test_runs, size, sess, sender, receiver, rtp_flags);
    }

    cleanup_ms(sess, sender);
    cleanup_ms(sess, receiver);
    cleanup_sess(ctx, sess);
}

TEST(FormatTests_ip6, h264_single_nal_unit_ip6)
{
    std::cout << "Starting IPv6 h264 Single NAL unit test" << std::endl;
    uvgrtp::context ctx;
    uvgrtp::session* sess = ctx.create_session(LOCAL_ADDRESS);

    uvgrtp::media_stream* sender = nullptr;
    uvgrtp::media_stream* receiver = nullptr;

    if (sess)
    {
        sender = sess->create_stream(SEND_PORT, RECEIVE_PORT, RTP_FORMAT_H264, RCE_NO_FLAGS);
        receiver = sess->create_stream(RECEIVE_PORT, SEND_PORT, RTP_FORMAT_H264, RCE_NO_FLAGS);
    }

    int rtp_flags = RTP_NO_FLAGS;
    rtp_format_t format = RTP_FORMAT_H264;
    int test_runs = 5;
    int size = 8;

    std::cout << "Testing small NAL unit" << std::endl;
    std::vector<size_t> test_sizes = std::vector<size_t>(16);
    std::iota(test_sizes.begin(), test_sizes.end(), 4);

    for (auto& size : test_sizes)
    {
        int nal_type = 8;
        std::unique_ptr<uint8_t[]> intra_frame = create_test_packet(format, nal_type, true, size, rtp_flags);
        test_packet_size(std::move(intra_frame), test_runs, size, sess, sender, receiver, rtp_flags);
    }

    size = 35;

    for (unsigned int nal_type = 1; nal_type <= 23; ++nal_type)
    {
        std::cout << "Testing H264 NAL type " << nal_type << std::endl;
        std::unique_ptr<uint8_t[]> intra_frame = create_test_packet(format, nal_type, true, size, rtp_flags);
        test_packet_size(std::move(intra_frame), test_runs, size, sess, sender, receiver, rtp_flags);
    }

    cleanup_ms(sess, sender);
    cleanup_ms(sess, receiver);
    cleanup_sess(ctx, sess);
}

TEST(FormatTests_ip6, h264_fragmentation_ip6)
{
    std::cout << "Starting IPv6 h264 fragmentation test" << std::endl;
    uvgrtp::context ctx;
    uvgrtp::session* sess = ctx.create_session(LOCAL_ADDRESS);

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

TEST(FormatTests_ip6, h265_single_nal_unit_ip6)
{
    std::cout << "Starting IPv6 H265 Single NAL unit test" << std::endl;
    uvgrtp::context ctx;
    uvgrtp::session* sess = ctx.create_session(LOCAL_ADDRESS);

    uvgrtp::media_stream* sender = nullptr;
    uvgrtp::media_stream* receiver = nullptr;

    if (sess)
    {
        sender = sess->create_stream(SEND_PORT, RECEIVE_PORT, RTP_FORMAT_H265, RCE_NO_FLAGS);
        receiver = sess->create_stream(RECEIVE_PORT, SEND_PORT, RTP_FORMAT_H265, RCE_NO_FLAGS);
    }

    int rtp_flags = RTP_NO_FLAGS;
    rtp_format_t format = RTP_FORMAT_H265;
    int test_runs = 5;
    int size = 8;

    std::cout << "Testing small NAL unit" << std::endl;
    std::vector<size_t> test_sizes = std::vector<size_t>(16);
    std::iota(test_sizes.begin(), test_sizes.end(), 6);

    for (auto& size : test_sizes)
    {
        int nal_type = 8;
        std::unique_ptr<uint8_t[]> intra_frame = create_test_packet(format, nal_type, true, size, rtp_flags);
        test_packet_size(std::move(intra_frame), test_runs, size, sess, sender, receiver, rtp_flags);
    }

    size = 35;

    for (unsigned int nal_type = 0; nal_type <= 47; ++nal_type)
    {
        std::cout << "Testing H264 NAL type " << nal_type << std::endl;
        std::unique_ptr<uint8_t[]> intra_frame = create_test_packet(format, nal_type, true, size, rtp_flags);
        test_packet_size(std::move(intra_frame), test_runs, size, sess, sender, receiver, rtp_flags);
    }

    cleanup_ms(sess, sender);
    cleanup_ms(sess, receiver);
    cleanup_sess(ctx, sess);
}

TEST(FormatTests_ip6, h265_fragmentation_ip6)
{
    std::cout << "Starting IPv6 h265 fragmentation test" << std::endl;
    uvgrtp::context ctx;
    uvgrtp::session* sess = ctx.create_session(LOCAL_ADDRESS);

    uvgrtp::media_stream* sender = nullptr;
    uvgrtp::media_stream* receiver = nullptr;

    if (sess)
    {
        sender = sess->create_stream(SEND_PORT, RECEIVE_PORT, RTP_FORMAT_H265, RCE_NO_FLAGS);
        receiver = sess->create_stream(RECEIVE_PORT, SEND_PORT, RTP_FORMAT_H265, RCE_NO_FLAGS);
    }

    std::vector<size_t> test_sizes = std::vector<size_t>(13);
    std::iota(test_sizes.begin(), test_sizes.end(), 1443);
    test_sizes.insert(test_sizes.end(), { 1501,
        1446 * 2 - 1,
        1446 * 2,
        1446 * 2 + 1,
        5000, 7500, 10000, 25000, 50000 });

    // the default packet limit for RTP is 1458 where 12 bytes are dedicated to RTP header
    int rtp_flags = RTP_NO_FLAGS;
    int nal_type = 5;
    rtp_format_t format = RTP_FORMAT_H265;
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

TEST(FormatTests_ip6, h265_fps_ip6)
{
    std::cout << "Starting IPv6 h265 test" << std::endl;
    uvgrtp::context ctx;
    uvgrtp::session* sess = ctx.create_session(LOCAL_ADDRESS);

    uvgrtp::media_stream* sender = nullptr;
    uvgrtp::media_stream* receiver = nullptr;

    int framerate = 10;

    if (sess)
    {
        sender = sess->create_stream(SEND_PORT, RECEIVE_PORT, RTP_FORMAT_H265, RCE_FRAME_RATE);
        receiver = sess->create_stream(RECEIVE_PORT, SEND_PORT, RTP_FORMAT_H265, RCE_NO_FLAGS);

        if (receiver)
        {
            sender->configure_ctx(RCC_FPS_NUMERATOR, framerate);
            sender->configure_ctx(RCC_FPS_DENOMINATOR, 1);
        }
    }

    std::vector<size_t> test_sizes = { 10000, 20000, 30000, 40000, 50000, 75000, 100000 };

    // the default packet limit for RTP is 1458 where 12 bytes are dedicated to RTP header
    int rtp_flags = RTP_NO_FLAGS;
    int nal_type = 5;
    rtp_format_t format = RTP_FORMAT_H265;
    int test_runs = 10;

    for (auto& size : test_sizes)
    {
        std::unique_ptr<uint8_t[]> intra_frame = create_test_packet(format, nal_type, true, size, rtp_flags);
        test_packet_size(std::move(intra_frame), test_runs, size, sess, sender, receiver, rtp_flags, framerate);
    }

    cleanup_ms(sess, sender);
    cleanup_ms(sess, receiver);
    cleanup_sess(ctx, sess);
}

TEST(FormatTests_ip6, h265_small_fragment_pacing_fps_ip6)
{
    std::cout << "Starting IPv6 h265 test" << std::endl;
    uvgrtp::context ctx;
    uvgrtp::session* sess = ctx.create_session(LOCAL_ADDRESS);

    uvgrtp::media_stream* sender = nullptr;
    uvgrtp::media_stream* receiver = nullptr;

    int framerate = 10;

    if (sess)
    {
        sender = sess->create_stream(SEND_PORT, RECEIVE_PORT, RTP_FORMAT_H265, RCE_FRAME_RATE | RCE_PACE_FRAGMENT_SENDING);
        receiver = sess->create_stream(RECEIVE_PORT, SEND_PORT, RTP_FORMAT_H265, RCE_NO_FLAGS);

        if (receiver)
        {
            sender->configure_ctx(RCC_FPS_NUMERATOR, framerate);
            sender->configure_ctx(RCC_FPS_DENOMINATOR, 1);

            receiver->configure_ctx(RCC_UDP_RCV_BUF_SIZE, 40 * 1000 * 1000);
            receiver->configure_ctx(RCC_RING_BUFFER_SIZE, 40 * 1000 * 1000);
        }
    }

    std::vector<size_t> test_sizes = { 1000, 2000, 3000, 4000, 5000, 7500, 10000 };

    // the default packet limit for RTP is 1458 where 12 bytes are dedicated to RTP header
    int rtp_flags = RTP_NO_FLAGS;
    int nal_type = 5;
    rtp_format_t format = RTP_FORMAT_H265;
    int test_runs = 10;

    for (auto& size : test_sizes)
    {
        std::unique_ptr<uint8_t[]> intra_frame = create_test_packet(format, nal_type, true, size, rtp_flags);
        test_packet_size(std::move(intra_frame), test_runs, size, sess, sender, receiver, rtp_flags, framerate);
    }

    cleanup_ms(sess, sender);
    cleanup_ms(sess, receiver);
    cleanup_sess(ctx, sess);
}

TEST(FormatTests_ip6, h266_single_nal_unit_ip6)
{
    std::cout << "Starting IPv6 H266 Single NAL unit test" << std::endl;
    uvgrtp::context ctx;
    uvgrtp::session* sess = ctx.create_session(LOCAL_ADDRESS);

    uvgrtp::media_stream* sender = nullptr;
    uvgrtp::media_stream* receiver = nullptr;

    if (sess)
    {
        sender = sess->create_stream(SEND_PORT, RECEIVE_PORT, RTP_FORMAT_H266, RCE_NO_FLAGS);
        receiver = sess->create_stream(RECEIVE_PORT, SEND_PORT, RTP_FORMAT_H266, RCE_NO_FLAGS);
    }

    int rtp_flags = RTP_NO_FLAGS;
    rtp_format_t format = RTP_FORMAT_H266;
    int test_runs = 5;
    int size = 8;

    std::cout << "Testing small NAL unit" << std::endl;
    std::vector<size_t> test_sizes = std::vector<size_t>(16);
    std::iota(test_sizes.begin(), test_sizes.end(), 6);

    for (auto& size : test_sizes)
    {
        int nal_type = 8;
        std::unique_ptr<uint8_t[]> intra_frame = create_test_packet(format, nal_type, true, size, rtp_flags);
        test_packet_size(std::move(intra_frame), test_runs, size, sess, sender, receiver, rtp_flags);
    }

    size = 35;

    for (unsigned int nal_type = 0; nal_type <= 27; ++nal_type)
    {
        std::cout << "Testing H264 NAL type " << nal_type << std::endl;
        std::unique_ptr<uint8_t[]> intra_frame = create_test_packet(format, nal_type, true, size, rtp_flags);
        test_packet_size(std::move(intra_frame), test_runs, size, sess, sender, receiver, rtp_flags);
    }

    cleanup_ms(sess, sender);
    cleanup_ms(sess, receiver);
    cleanup_sess(ctx, sess);
}

TEST(FormatTests_ip6, h265_large_fragment_pacing_ip6)
{
    std::cout << "Starting IPv6 h265 test" << std::endl;
    uvgrtp::context ctx;
    uvgrtp::session* sess = ctx.create_session(LOCAL_ADDRESS);

    uvgrtp::media_stream* sender = nullptr;
    uvgrtp::media_stream* receiver = nullptr;

    int framerate = 10;

    if (sess)
    {
        sender = sess->create_stream(SEND_PORT, RECEIVE_PORT, RTP_FORMAT_H265, RCE_PACE_FRAGMENT_SENDING);
        receiver = sess->create_stream(RECEIVE_PORT, SEND_PORT, RTP_FORMAT_H265, RCE_NO_FLAGS);

        if (receiver)
        {
            sender->configure_ctx(RCC_FPS_NUMERATOR, framerate);
            sender->configure_ctx(RCC_FPS_DENOMINATOR, 1);

            receiver->configure_ctx(RCC_UDP_RCV_BUF_SIZE, 40 * 1000 * 1000);
            receiver->configure_ctx(RCC_RING_BUFFER_SIZE, 40 * 1000 * 1000);
        }
    }

    std::vector<size_t> test_sizes = { 100000, 200000, 300000, 400000, 500000, 750000, 1000000 };

    // the default packet limit for RTP is 1458 where 12 bytes are dedicated to RTP header
    int rtp_flags = RTP_NO_FLAGS;
    int nal_type = 5;
    rtp_format_t format = RTP_FORMAT_H265;
    int test_runs = 10;

    for (auto& size : test_sizes)
    {
        std::unique_ptr<uint8_t[]> intra_frame = create_test_packet(format, nal_type, true, size, rtp_flags);
        test_packet_size(std::move(intra_frame), test_runs, size, sess, sender, receiver, rtp_flags, framerate);
    }

    cleanup_ms(sess, sender);
    cleanup_ms(sess, receiver);
    cleanup_sess(ctx, sess);
}

TEST(FormatTests_ip6, h266_fragmentation_ip6)
{
    std::cout << "Starting IPv6 h266 fragmentation test" << std::endl;
    uvgrtp::context ctx;
    uvgrtp::session* sess = ctx.create_session(LOCAL_ADDRESS);

    uvgrtp::media_stream* sender = nullptr;
    uvgrtp::media_stream* receiver = nullptr;

    if (sess)
    {
        sender = sess->create_stream(SEND_PORT, RECEIVE_PORT, RTP_FORMAT_H266, RCE_NO_FLAGS);
        receiver = sess->create_stream(RECEIVE_PORT, SEND_PORT, RTP_FORMAT_H266, RCE_NO_FLAGS);
    }

    std::vector<size_t> test_sizes = std::vector<size_t>(13);
    std::iota(test_sizes.begin(), test_sizes.end(), 1443);
    test_sizes.insert(test_sizes.end(), { 1501,
        1446 * 2 - 1,
        1446 * 2,
        1446 * 2 + 1,
        5000, 7500, 10000, 25000, 50000 });

    // the default packet limit for RTP is 1458 where 12 bytes are dedicated to RTP header
    int rtp_flags = RTP_NO_FLAGS;
    int nal_type = 5;
    rtp_format_t format = RTP_FORMAT_H266;
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