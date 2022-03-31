#include "test_common.hh"

constexpr uint16_t SEND_PORT = 9100;
constexpr char LOCAL_ADDRESS[] = "127.0.0.1";
constexpr uint16_t RECEIVE_PORT = 9102;

constexpr int AMOUNT_OF_TEST_PACKETS = 100;
constexpr size_t PAYLOAD_LEN = 100;

// TODO: Use real files

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
        receiver = sess->create_stream(RECEIVE_PORT, SEND_PORT, RTP_FORMAT_H264, RCE_H26X_PREPEND_SC);
    }

    // the default packet limit for RTP is 1458 where 12 bytes are dedicated to RTP header
    test_packet_size(1443, sess, sender, receiver);
    test_packet_size(1444, sess, sender, receiver);
    test_packet_size(1445, sess, sender, receiver);
    test_packet_size(1446, sess, sender, receiver); // packet limit
    test_packet_size(1447, sess, sender, receiver);
    test_packet_size(1448, sess, sender, receiver);
    test_packet_size(1449, sess, sender, receiver);
    test_packet_size(1450, sess, sender, receiver);
    test_packet_size(1451, sess, sender, receiver);
    test_packet_size(1452, sess, sender, receiver);
    test_packet_size(1453, sess, sender, receiver);
    test_packet_size(1454, sess, sender, receiver);
    test_packet_size(1455, sess, sender, receiver);
    test_packet_size(1501, sess, sender, receiver);
    test_packet_size(15000, sess, sender, receiver);
    test_packet_size(150000, sess, sender, receiver);

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
    test_packet_size(1443, sess, sender, receiver);
    test_packet_size(1444, sess, sender, receiver);
    test_packet_size(1445, sess, sender, receiver);
    test_packet_size(1446, sess, sender, receiver); // packet limit
    test_packet_size(1447, sess, sender, receiver);
    test_packet_size(1448, sess, sender, receiver);
    test_packet_size(1449, sess, sender, receiver);
    test_packet_size(1450, sess, sender, receiver);
    test_packet_size(1451, sess, sender, receiver);
    test_packet_size(1452, sess, sender, receiver);
    test_packet_size(1453, sess, sender, receiver);
    test_packet_size(1454, sess, sender, receiver);
    test_packet_size(1455, sess, sender, receiver);
    test_packet_size(1501, sess, sender, receiver);
    test_packet_size(15000, sess, sender, receiver);
    test_packet_size(150000, sess, sender, receiver);

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
    test_packet_size(1443, sess, sender, receiver);
    test_packet_size(1444, sess, sender, receiver);
    test_packet_size(1445, sess, sender, receiver);
    test_packet_size(1446, sess, sender, receiver); // packet limit
    test_packet_size(1447, sess, sender, receiver);
    test_packet_size(1448, sess, sender, receiver);
    test_packet_size(1449, sess, sender, receiver);
    test_packet_size(1450, sess, sender, receiver);
    test_packet_size(1451, sess, sender, receiver);
    test_packet_size(1452, sess, sender, receiver);
    test_packet_size(1453, sess, sender, receiver);
    test_packet_size(1454, sess, sender, receiver);
    test_packet_size(1455, sess, sender, receiver);
    test_packet_size(1501, sess, sender, receiver);
    test_packet_size(15000, sess, sender, receiver);
    test_packet_size(150000, sess, sender, receiver);

    cleanup_ms(sess, sender);
    cleanup_ms(sess, receiver);
    cleanup_sess(ctx, sess);
}