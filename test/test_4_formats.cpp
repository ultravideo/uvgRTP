#include "lib.hh"
#include <gtest/gtest.h>

constexpr uint16_t SEND_PORT = 9100;
constexpr char LOCAL_ADDRESS[] = "127.0.0.1";
constexpr uint16_t RECEIVE_PORT = 9102;

constexpr int AMOUNT_OF_TEST_PACKETS = 100;
constexpr size_t PAYLOAD_LEN = 100;


void format_receive_hook(void* arg, uvgrtp::frame::rtp_frame* frame);
void cleanup_formats(uvgrtp::context& ctx, uvgrtp::session* sess, uvgrtp::media_stream* ms);
void process_format_frame(uvgrtp::frame::rtp_frame* frame);


// TODO: Use real files

TEST(FormatTests, h264)
{
    std::cout << "Starting h264 test" << std::endl;
    uvgrtp::context ctx;
    uvgrtp::session* sess = ctx.create_session(LOCAL_ADDRESS);

    uvgrtp::media_stream* sender = nullptr;
    uvgrtp::media_stream* receiver = nullptr;

    int flags = RTP_NO_FLAGS;
    if (sess)
    {
        sender = sess->create_stream(SEND_PORT, RECEIVE_PORT, RTP_FORMAT_H264, flags);
        receiver = sess->create_stream(RECEIVE_PORT, SEND_PORT, RTP_FORMAT_H264, flags);
    }

    if (receiver)
    {
        std::cout << "Installing hook" << std::endl;
        EXPECT_EQ(RTP_OK, receiver->install_receive_hook(nullptr, format_receive_hook));
    }

    if (sender)
    {
        for (int i = 0; i < AMOUNT_OF_TEST_PACKETS; ++i)
        {
            std::unique_ptr<uint8_t[]> dummy_frame = std::unique_ptr<uint8_t[]>(new uint8_t[PAYLOAD_LEN]);

            if ((i + 1) % 10 == 0 || i == 0) // print every 10 frames and first
                std::cout << "Sending frame " << i + 1 << '/' << AMOUNT_OF_TEST_PACKETS << std::endl;

            if (sender->push_frame(std::move(dummy_frame), PAYLOAD_LEN, RTP_NO_FLAGS) != RTP_OK)
            {
                std::cout << "Failed to send RTP frame!" << std::endl;
            }
        }

        EXPECT_NE(nullptr, sender);
        sess->destroy_stream(sender);
        sender = nullptr;
    }

    EXPECT_NE(nullptr, sess);
    EXPECT_NE(nullptr, receiver);

    cleanup_formats(ctx, sess, receiver);
}

TEST(FormatTests, h265)
{
    std::cout << "Starting h265 test" << std::endl;
    uvgrtp::context ctx;
    uvgrtp::session* sess = ctx.create_session(LOCAL_ADDRESS);

    uvgrtp::media_stream* sender = nullptr;
    uvgrtp::media_stream* receiver = nullptr;

    int flags = RTP_NO_FLAGS;
    if (sess)
    {
        sender = sess->create_stream(SEND_PORT, RECEIVE_PORT, RTP_FORMAT_H265, flags);
        receiver = sess->create_stream(RECEIVE_PORT, SEND_PORT, RTP_FORMAT_H265, flags);
    }

    if (receiver)
    {
        std::cout << "Installing hook" << std::endl;
        EXPECT_EQ(RTP_OK, receiver->install_receive_hook(nullptr, format_receive_hook));
    }

    if (sender)
    {
        for (int i = 0; i < AMOUNT_OF_TEST_PACKETS; ++i)
        {
            std::unique_ptr<uint8_t[]> dummy_frame = std::unique_ptr<uint8_t[]>(new uint8_t[PAYLOAD_LEN]);

            if ((i + 1) % 10 == 0 || i == 0) // print every 10 frames and first
                std::cout << "Sending frame " << i + 1 << '/' << AMOUNT_OF_TEST_PACKETS << std::endl;

            if (sender->push_frame(std::move(dummy_frame), PAYLOAD_LEN, RTP_NO_FLAGS) != RTP_OK)
            {
                std::cout << "Failed to send RTP frame!" << std::endl;
            }
        }

        EXPECT_NE(nullptr, sender);
        sess->destroy_stream(sender);
        sender = nullptr;
    }

    EXPECT_NE(nullptr, sess);
    EXPECT_NE(nullptr, receiver);

    cleanup_formats(ctx, sess, receiver);
}

TEST(FormatTests, h266)
{
    std::cout << "Starting h266 test" << std::endl;
    uvgrtp::context ctx;
    uvgrtp::session* sess = ctx.create_session(LOCAL_ADDRESS);

    uvgrtp::media_stream* sender = nullptr;
    uvgrtp::media_stream* receiver = nullptr;

    int flags = RTP_NO_FLAGS;
    if (sess)
    {
        sender = sess->create_stream(SEND_PORT, RECEIVE_PORT, RTP_FORMAT_H266, flags);
        receiver = sess->create_stream(RECEIVE_PORT, SEND_PORT, RTP_FORMAT_H266, flags);
    }

    if (receiver)
    {
        std::cout << "Installing hook" << std::endl;
        EXPECT_EQ(RTP_OK, receiver->install_receive_hook(nullptr, format_receive_hook));
    }

    if (sender)
    {
        for (int i = 0; i < AMOUNT_OF_TEST_PACKETS; ++i)
        {
            std::unique_ptr<uint8_t[]> dummy_frame = std::unique_ptr<uint8_t[]>(new uint8_t[PAYLOAD_LEN]);

            if ((i + 1) % 10 == 0 || i == 0) // print every 10 frames and first
                std::cout << "Sending frame " << i + 1 << '/' << AMOUNT_OF_TEST_PACKETS << std::endl;

            if (sender->push_frame(std::move(dummy_frame), PAYLOAD_LEN, RTP_NO_FLAGS) != RTP_OK)
            {
                std::cout << "Failed to send RTP frame!" << std::endl;
            }
        }

        EXPECT_NE(nullptr, sender);
        sess->destroy_stream(sender);
        sender = nullptr;
    }

    EXPECT_NE(nullptr, sess);
    EXPECT_NE(nullptr, receiver);

    cleanup_formats(ctx, sess, receiver);
}

void format_receive_hook(void* arg, uvgrtp::frame::rtp_frame* frame)
{
    process_format_frame(frame);
}

void cleanup_formats(uvgrtp::context& ctx, uvgrtp::session* sess, uvgrtp::media_stream* ms)
{
    if (ms)
    {
        sess->destroy_stream(ms);
    }

    if (sess)
    {
        ctx.destroy_session(sess);
    }
}

void process_format_frame(uvgrtp::frame::rtp_frame* frame)
{
    std::cout << "Receiving frame with seq: " << frame->header.seq << std::endl;

    EXPECT_EQ(PAYLOAD_LEN, frame->payload_len);
    (void)uvgrtp::frame::dealloc_frame(frame);
}