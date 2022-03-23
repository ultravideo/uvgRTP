#include "test_common.hh"

constexpr uint16_t SEND_PORT = 9100;
constexpr char LOCAL_ADDRESS[] = "127.0.0.1";
constexpr uint16_t RECEIVE_PORT = 9102;

constexpr int AMOUNT_OF_TEST_PACKETS = 100;
constexpr size_t PAYLOAD_LEN = 100;


void format_receive_hook(void* arg, uvgrtp::frame::rtp_frame* frame);
void process_format_frame(uvgrtp::frame::rtp_frame* frame);


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

    add_hook(nullptr, receiver, format_receive_hook);
    send_packets(sess, sender, AMOUNT_OF_TEST_PACKETS, PAYLOAD_LEN, 0, true, false);

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

    add_hook(nullptr, receiver, format_receive_hook);
    send_packets(sess, sender, AMOUNT_OF_TEST_PACKETS, PAYLOAD_LEN, 0, true, false);

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

    add_hook(nullptr, receiver, format_receive_hook);
    send_packets(sess, sender, AMOUNT_OF_TEST_PACKETS, PAYLOAD_LEN, 0, true, false);

    cleanup_ms(sess, sender);
    cleanup_ms(sess, receiver);
    cleanup_sess(ctx, sess);
}

void format_receive_hook(void* arg, uvgrtp::frame::rtp_frame* frame)
{
    process_format_frame(frame);
}

void process_format_frame(uvgrtp::frame::rtp_frame* frame)
{
    //std::cout << "Receiving frame with seq: " << frame->header.seq 
    //    << " and timestamp: " << frame->header.timestamp <<  std::endl;

    EXPECT_EQ(frame->header.version, 2);
    EXPECT_EQ(PAYLOAD_LEN, frame->payload_len);
    (void)uvgrtp::frame::dealloc_frame(frame);
}