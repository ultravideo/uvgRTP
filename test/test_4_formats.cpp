#include "test_common.hh"

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

    test_packet_size(RTP_FORMAT_H264, 10, 1443, sess, sender, receiver, RTP_NO_FLAGS);
    test_packet_size(RTP_FORMAT_H264, 10, 1501, sess, sender, receiver, RTP_NO_FLAGS);

    test_packet_size(RTP_FORMAT_H264, 10, 1443, sess, sender, receiver, RTP_NO_H26X_SCL);
    test_packet_size(RTP_FORMAT_H264, 10, 1501, sess, sender, receiver, RTP_NO_H26X_SCL);

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
    test_packet_size(RTP_FORMAT_H264, 10, 1443, sess, sender, receiver, RTP_NO_FLAGS);
    test_packet_size(RTP_FORMAT_H264, 10, 1444, sess, sender, receiver, RTP_NO_FLAGS);
    test_packet_size(RTP_FORMAT_H264, 10, 1445, sess, sender, receiver, RTP_NO_FLAGS);
    test_packet_size(RTP_FORMAT_H264, 10, 1446, sess, sender, receiver, RTP_NO_FLAGS); // packet limit
    test_packet_size(RTP_FORMAT_H264, 10, 1447, sess, sender, receiver, RTP_NO_FLAGS);
    test_packet_size(RTP_FORMAT_H264, 10, 1448, sess, sender, receiver, RTP_NO_FLAGS);
    test_packet_size(RTP_FORMAT_H264, 10, 1449, sess, sender, receiver, RTP_NO_FLAGS);
    test_packet_size(RTP_FORMAT_H264, 10, 1450, sess, sender, receiver, RTP_NO_FLAGS);
    test_packet_size(RTP_FORMAT_H264, 10, 1451, sess, sender, receiver, RTP_NO_FLAGS);
    test_packet_size(RTP_FORMAT_H264, 10, 1452, sess, sender, receiver, RTP_NO_FLAGS);
    test_packet_size(RTP_FORMAT_H264, 10, 1453, sess, sender, receiver, RTP_NO_FLAGS);
    test_packet_size(RTP_FORMAT_H264, 10, 1454, sess, sender, receiver, RTP_NO_FLAGS);
    test_packet_size(RTP_FORMAT_H264, 10, 1455, sess, sender, receiver, RTP_NO_FLAGS);
    test_packet_size(RTP_FORMAT_H264, 10, 1501, sess, sender, receiver, RTP_NO_FLAGS);

    test_packet_size(RTP_FORMAT_H264, 10, 1446 * 2 - 1, sess, sender, receiver, RTP_NO_FLAGS);
    test_packet_size(RTP_FORMAT_H264, 10, 1446 * 2, sess, sender, receiver, RTP_NO_FLAGS);
    test_packet_size(RTP_FORMAT_H264, 10, 1446 * 2 + 1, sess, sender, receiver, RTP_NO_FLAGS);

    test_packet_size(RTP_FORMAT_H264, 10, 5000, sess, sender, receiver, RTP_NO_FLAGS);
    test_packet_size(RTP_FORMAT_H264, 10, 7500, sess, sender, receiver, RTP_NO_FLAGS);
    test_packet_size(RTP_FORMAT_H264, 10, 10000, sess, sender, receiver, RTP_NO_FLAGS);
    test_packet_size(RTP_FORMAT_H264, 10, 25000, sess, sender, receiver, RTP_NO_FLAGS);
    test_packet_size(RTP_FORMAT_H264, 10, 50000, sess, sender, receiver, RTP_NO_FLAGS);

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

    // the default packet limit for RTP is 1458 where 12 bytes are dedicated to RTP header
    test_packet_size(RTP_FORMAT_H265, 10, 1443, sess, sender, receiver, RTP_NO_FLAGS);
    test_packet_size(RTP_FORMAT_H265, 10, 1444, sess, sender, receiver, RTP_NO_FLAGS);
    test_packet_size(RTP_FORMAT_H265, 10, 1445, sess, sender, receiver, RTP_NO_FLAGS);
    test_packet_size(RTP_FORMAT_H265, 10, 1446, sess, sender, receiver, RTP_NO_FLAGS); // packet limit
    test_packet_size(RTP_FORMAT_H265, 10, 1447, sess, sender, receiver, RTP_NO_FLAGS);
    test_packet_size(RTP_FORMAT_H265, 10, 1448, sess, sender, receiver, RTP_NO_FLAGS);
    test_packet_size(RTP_FORMAT_H265, 10, 1449, sess, sender, receiver, RTP_NO_FLAGS);
    test_packet_size(RTP_FORMAT_H265, 10, 1450, sess, sender, receiver, RTP_NO_FLAGS);
    test_packet_size(RTP_FORMAT_H265, 10, 1451, sess, sender, receiver, RTP_NO_FLAGS);
    test_packet_size(RTP_FORMAT_H265, 10, 1452, sess, sender, receiver, RTP_NO_FLAGS);
    test_packet_size(RTP_FORMAT_H265, 10, 1453, sess, sender, receiver, RTP_NO_FLAGS);
    test_packet_size(RTP_FORMAT_H265, 10, 1454, sess, sender, receiver, RTP_NO_FLAGS);
    test_packet_size(RTP_FORMAT_H265, 10, 1455, sess, sender, receiver, RTP_NO_FLAGS);
    test_packet_size(RTP_FORMAT_H265, 10, 1501, sess, sender, receiver, RTP_NO_FLAGS);

    test_packet_size(RTP_FORMAT_H265, 10, 1446 * 2 - 1, sess, sender, receiver, RTP_NO_FLAGS);
    test_packet_size(RTP_FORMAT_H265, 10, 1446 * 2, sess, sender, receiver, RTP_NO_FLAGS);
    test_packet_size(RTP_FORMAT_H265, 10, 1446 * 2 + 1, sess, sender, receiver, RTP_NO_FLAGS);

    test_packet_size(RTP_FORMAT_H265, 10, 5000, sess, sender, receiver, RTP_NO_FLAGS);
    test_packet_size(RTP_FORMAT_H265, 10, 7500, sess, sender, receiver, RTP_NO_FLAGS);
    test_packet_size(RTP_FORMAT_H265, 10, 10000, sess, sender, receiver, RTP_NO_FLAGS);
    test_packet_size(RTP_FORMAT_H265, 10, 25000, sess, sender, receiver, RTP_NO_FLAGS);
    test_packet_size(RTP_FORMAT_H265, 10, 50000, sess, sender, receiver, RTP_NO_FLAGS);

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
    }

    test_packet_size(RTP_FORMAT_H265, 10,  100000, sess, sender, receiver, RTP_NO_FLAGS);
    test_packet_size(RTP_FORMAT_H265, 10,  200000, sess, sender, receiver, RTP_NO_FLAGS);
    test_packet_size(RTP_FORMAT_H265, 10,  300000, sess, sender, receiver, RTP_NO_FLAGS);
    test_packet_size(RTP_FORMAT_H265, 10,  400000, sess, sender, receiver, RTP_NO_FLAGS);
    test_packet_size(RTP_FORMAT_H265, 10,  500000, sess, sender, receiver, RTP_NO_FLAGS);
    test_packet_size(RTP_FORMAT_H265, 10,  750000, sess, sender, receiver, RTP_NO_FLAGS);
    test_packet_size(RTP_FORMAT_H265, 10, 1000000, sess, sender, receiver, RTP_NO_FLAGS);

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

    // the default packet limit for RTP is 1458 where 12 bytes are dedicated to RTP header
    test_packet_size(RTP_FORMAT_H266, 10, 1443, sess, sender, receiver, RTP_NO_FLAGS);
    test_packet_size(RTP_FORMAT_H266, 10, 1444, sess, sender, receiver, RTP_NO_FLAGS);
    test_packet_size(RTP_FORMAT_H266, 10, 1445, sess, sender, receiver, RTP_NO_FLAGS);
    test_packet_size(RTP_FORMAT_H266, 10, 1446, sess, sender, receiver, RTP_NO_FLAGS); // packet limit
    test_packet_size(RTP_FORMAT_H266, 10, 1447, sess, sender, receiver, RTP_NO_FLAGS);
    test_packet_size(RTP_FORMAT_H266, 10, 1448, sess, sender, receiver, RTP_NO_FLAGS);
    test_packet_size(RTP_FORMAT_H266, 10, 1449, sess, sender, receiver, RTP_NO_FLAGS);
    test_packet_size(RTP_FORMAT_H266, 10, 1450, sess, sender, receiver, RTP_NO_FLAGS);
    test_packet_size(RTP_FORMAT_H266, 10, 1451, sess, sender, receiver, RTP_NO_FLAGS);
    test_packet_size(RTP_FORMAT_H266, 10, 1452, sess, sender, receiver, RTP_NO_FLAGS);
    test_packet_size(RTP_FORMAT_H266, 10, 1453, sess, sender, receiver, RTP_NO_FLAGS);
    test_packet_size(RTP_FORMAT_H266, 10, 1454, sess, sender, receiver, RTP_NO_FLAGS);
    test_packet_size(RTP_FORMAT_H266, 10, 1455, sess, sender, receiver, RTP_NO_FLAGS);
    test_packet_size(RTP_FORMAT_H266, 10, 1501, sess, sender, receiver, RTP_NO_FLAGS);

    test_packet_size(RTP_FORMAT_H266, 10, 1446 * 2 - 1, sess, sender, receiver, RTP_NO_FLAGS);
    test_packet_size(RTP_FORMAT_H266, 10, 1446 * 2,     sess, sender, receiver, RTP_NO_FLAGS);
    test_packet_size(RTP_FORMAT_H266, 10, 1446 * 2 + 1, sess, sender, receiver, RTP_NO_FLAGS);

    test_packet_size(RTP_FORMAT_H266, 10, 5000, sess, sender, receiver, RTP_NO_FLAGS);
    test_packet_size(RTP_FORMAT_H266, 10, 7500, sess, sender, receiver, RTP_NO_FLAGS);
    test_packet_size(RTP_FORMAT_H266, 10, 10000, sess, sender, receiver, RTP_NO_FLAGS);
    test_packet_size(RTP_FORMAT_H266, 10, 25000, sess, sender, receiver, RTP_NO_FLAGS);
    test_packet_size(RTP_FORMAT_H266, 10, 50000, sess, sender, receiver, RTP_NO_FLAGS);

    cleanup_ms(sess, sender);
    cleanup_ms(sess, receiver);
    cleanup_sess(ctx, sess);
}