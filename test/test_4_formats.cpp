#include "test_common.hh"

#include <numeric>

constexpr uint16_t SEND_PORT = 9100;
constexpr char LOCAL_ADDRESS[] = "127.0.0.1";
constexpr uint16_t RECEIVE_PORT = 9102;

constexpr int AMOUNT_OF_TEST_PACKETS = 100;
constexpr size_t PAYLOAD_LEN = 100;

// TODO: Use real files

TEST(FormatTests, h26x_flags)
{
    std::cout << "Starting h26x flag test" << std::endl;

    uvgrtp::context ctx;
    uvgrtp::session* sess = ctx.create_session(LOCAL_ADDRESS);

    uvgrtp::media_stream* sender = nullptr;
    uvgrtp::media_stream* receiver = nullptr;

    if (sess)
    {
        sender = sess->create_stream(SEND_PORT, RECEIVE_PORT, RTP_FORMAT_H265, RCE_NO_FLAGS);
        receiver = sess->create_stream(RECEIVE_PORT, SEND_PORT, RTP_FORMAT_H265, RCE_H26X_PREPEND_SC);
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

TEST(FormatTests, h264)
{
    std::cout << "Starting h264 test" << std::endl;
    uvgrtp::context ctx;
    uvgrtp::session* sess = ctx.create_session(LOCAL_ADDRESS);

    uvgrtp::media_stream* sender = nullptr;
    uvgrtp::media_stream* receiver = nullptr;

    if (sess)
    {
        sender = sess->create_stream(SEND_PORT, RECEIVE_PORT, RTP_FORMAT_H264, RCE_NO_FLAGS);
        receiver = sess->create_stream(RECEIVE_PORT, SEND_PORT, 
            RTP_FORMAT_H264, RCE_H26X_PREPEND_SC);
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

TEST(FormatTests, h265)
{
    std::cout << "Starting h265 test" << std::endl;
    uvgrtp::context ctx;
    uvgrtp::session* sess = ctx.create_session(LOCAL_ADDRESS);

    uvgrtp::media_stream* sender = nullptr;
    uvgrtp::media_stream* receiver = nullptr;

    if (sess)
    {
        sender = sess->create_stream(SEND_PORT, RECEIVE_PORT, RTP_FORMAT_H265, RCE_NO_FLAGS);
        receiver = sess->create_stream(RECEIVE_PORT, SEND_PORT, RTP_FORMAT_H265, RCE_H26X_PREPEND_SC);
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

TEST(FormatTests, h265_large)
{
    std::cout << "Starting h265 test" << std::endl;
    uvgrtp::context ctx;
    uvgrtp::session* sess = ctx.create_session(LOCAL_ADDRESS);

    uvgrtp::media_stream* sender = nullptr;
    uvgrtp::media_stream* receiver = nullptr;

    if (sess)
    {
        sender = sess->create_stream(SEND_PORT, RECEIVE_PORT, RTP_FORMAT_H265, RCE_NO_FLAGS);
        receiver = sess->create_stream(RECEIVE_PORT, SEND_PORT, RTP_FORMAT_H265, RCE_H26X_PREPEND_SC);
        
        if (receiver)
        {
            receiver->configure_ctx(RCC_UDP_RCV_BUF_SIZE, 40 * 1000 * 1000);
        }
    }

    std::vector<size_t> test_sizes = {100000, 200000, 300000, 400000, 500000, 750000, 1000000};

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

TEST(FormatTests, h265_large_fps)
{
    std::cout << "Starting h265 test" << std::endl;
    uvgrtp::context ctx;
    uvgrtp::session* sess = ctx.create_session(LOCAL_ADDRESS);

    uvgrtp::media_stream* sender = nullptr;
    uvgrtp::media_stream* receiver = nullptr;

    if (sess)
    {
        sender = sess->create_stream(SEND_PORT, RECEIVE_PORT, RTP_FORMAT_H265, RCE_NO_FLAGS);
        receiver = sess->create_stream(RECEIVE_PORT, SEND_PORT, RTP_FORMAT_H265, RCE_H26X_PREPEND_SC);

        if (receiver)
        {
            sender->configure_ctx(RCC_FPS_ENUM, 100);
            sender->configure_ctx(RCC_FPS_DENUM, 1);

            receiver->configure_ctx(RCC_UDP_RCV_BUF_SIZE, 40 * 1000 * 1000);
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
        test_packet_size(std::move(intra_frame), test_runs, size, sess, sender, receiver, rtp_flags);
    }

    cleanup_ms(sess, sender);
    cleanup_ms(sess, receiver);
    cleanup_sess(ctx, sess);
}

TEST(FormatTests, h266)
{
    std::cout << "Starting h266 test" << std::endl;
    uvgrtp::context ctx;
    uvgrtp::session* sess = ctx.create_session(LOCAL_ADDRESS);

    uvgrtp::media_stream* sender = nullptr;
    uvgrtp::media_stream* receiver = nullptr;

    if (sess)
    {
        sender = sess->create_stream(SEND_PORT, RECEIVE_PORT, RTP_FORMAT_H266, RCE_NO_FLAGS);
        receiver = sess->create_stream(RECEIVE_PORT, SEND_PORT, RTP_FORMAT_H266, RCE_H26X_PREPEND_SC);
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