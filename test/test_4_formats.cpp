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

TEST(FormatTests, h264_single_nal_unit)
{
    std::cout << "Starting h264 Single NAL unit test" << std::endl;
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

TEST(FormatTests, h264_fragmentation)
{
    std::cout << "Starting h264 fragmentation test" << std::endl;
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

TEST(FormatTests, h265_single_nal_unit)
{
    std::cout << "Starting H265 Single NAL unit test" << std::endl;
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

TEST(FormatTests, h265_fragmentation)
{
    std::cout << "Starting h265 fragmentation test" << std::endl;
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

TEST(FormatTests, h265_fps)
{
    std::cout << "Starting h265 test" << std::endl;
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

TEST(FormatTests, h265_small_fragment_pacing_fps)
{
    std::cout << "Starting h265 test" << std::endl;
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

TEST(FormatTests, h266_single_nal_unit)
{
    std::cout << "Starting H266 Single NAL unit test" << std::endl;
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

TEST(FormatTests, h265_large_fragment_pacing)
{
    std::cout << "Starting h265 test" << std::endl;
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
            /* This is so high because on the gitlab CI tests the performance seems to be quite bad so extra time is needed */
            receiver->configure_ctx(RCC_PKT_MAX_DELAY, 2500);
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

TEST(FormatTests, h266_fragmentation)
{
    std::cout << "Starting h266 fragmentation test" << std::endl;
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

TEST(FormatTests, h264_multiplex)
{
    // Test multiplexing two RTP streams into a single socket, format H264 fragmentation
    std::cout << "Starting H264 fragmentation multiplexing test" << std::endl;
    uvgrtp::context ctx;
    uvgrtp::session* receiver_sess = ctx.create_session(LOCAL_ADDRESS);
    uvgrtp::session* sender_sess = ctx.create_session(LOCAL_ADDRESS);

    uvgrtp::media_stream* sender1 = nullptr;
    uvgrtp::media_stream* receiver1 = nullptr;
    uvgrtp::media_stream* sender2 = nullptr;
    uvgrtp::media_stream* receiver2 = nullptr;

    int flags = RCE_FRAGMENT_GENERIC;
    if (sender_sess)
    {
        sender1 = sender_sess->create_stream(RECEIVE_PORT, SEND_PORT, RTP_FORMAT_H264, flags);
        sender1->configure_ctx(RCC_SSRC, 11);
        sender1->configure_ctx(RCC_REMOTE_SSRC, 22);
        sender2 = sender_sess->create_stream(RECEIVE_PORT, SEND_PORT, RTP_FORMAT_H264, flags);
        sender2->configure_ctx(RCC_SSRC, 33);
        sender2->configure_ctx(RCC_REMOTE_SSRC, 44);
    }
    if (receiver_sess)
    {
        receiver1 = receiver_sess->create_stream(SEND_PORT, RECEIVE_PORT, RTP_FORMAT_H264, flags);
        receiver1->configure_ctx(RCC_SSRC, 22);
        receiver1->configure_ctx(RCC_REMOTE_SSRC, 11);
        receiver2 = receiver_sess->create_stream(SEND_PORT, RECEIVE_PORT, RTP_FORMAT_H264, flags);
        receiver2->configure_ctx(RCC_SSRC, 44);
        receiver2->configure_ctx(RCC_REMOTE_SSRC, 33);
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
        std::unique_ptr<uint8_t[]> intra_frame1 = create_test_packet(format, nal_type, true, size, rtp_flags);
        std::unique_ptr<uint8_t[]> intra_frame2 = create_test_packet(format, nal_type, true, size, rtp_flags);
        test_packet_size(std::move(intra_frame1), test_runs, size, sender_sess, sender1, receiver1, rtp_flags);
        test_packet_size(std::move(intra_frame2), test_runs, size, sender_sess, sender2, receiver2, rtp_flags);
    }

    cleanup_ms(sender_sess, sender1);
    cleanup_ms(sender_sess, sender2);
    cleanup_ms(receiver_sess, receiver1);
    cleanup_ms(receiver_sess, receiver2);
    cleanup_sess(ctx, sender_sess);
    cleanup_sess(ctx, receiver_sess);

}