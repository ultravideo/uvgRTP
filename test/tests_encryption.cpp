#include "lib.hh"
#include <gtest/gtest.h>


// network parameters of example
constexpr char SENDER_ADDRESS[] = "127.0.0.1";
constexpr uint16_t LOCAL_PORT = 9000;

constexpr char RECEIVER_ADDRESS[] = "127.0.0.1";
constexpr uint16_t REMOTE_PORT = 9002;

constexpr auto EXAMPLE_DURATION = std::chrono::seconds(1);

// encryption parameters of example
enum Key_length { SRTP_128 = 128, SRTP_196 = 196, SRTP_256 = 256 };
constexpr Key_length KEY_SIZE = SRTP_256;
constexpr int KEY_SIZE_BYTES = KEY_SIZE / 8;
constexpr int SALT_SIZE = 112;
constexpr int SALT_SIZE_BYTES = SALT_SIZE / 8;

void process_frame(uvgrtp::frame::rtp_frame* frame);
void receive_func(uint8_t key[KEY_SIZE_BYTES], uint8_t salt[SALT_SIZE_BYTES]);


std::unique_ptr<std::thread> user_initialization(uvgrtp::context& ctx, Key_length sha,
    uvgrtp::session* sender_session, uvgrtp::media_stream* send);

TEST(EncryptionTests, no_send_user)
{
    uvgrtp::context ctx;

    uvgrtp::session* sender_session = nullptr; 
    uvgrtp::media_stream* send = nullptr;
    std::unique_ptr<std::thread> receiver;

    receiver = user_initialization(ctx, SRTP_256, sender_session, send);

    // All media is now encrypted/decrypted automatically
    char* message = (char*)"Hello, world!";
    size_t msg_len = strlen(message);

    if (send)
    {
        auto start = std::chrono::steady_clock::now();
        for (unsigned int i = 0; i < 10; ++i)
        {
            EXPECT_EQ(RTP_OK, send->push_frame((uint8_t*)message, msg_len, RTP_NO_FLAGS));
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

    sender_session->destroy_stream(send);

    if (receiver && receiver->joinable())
    {
        receiver->join();
    }

    if (sender_session)
        ctx.destroy_session(sender_session);
}

std::unique_ptr<std::thread> user_initialization(uvgrtp::context& ctx, Key_length sha, 
    uvgrtp::session* sender_session, uvgrtp::media_stream* send)
{
    uint8_t key[KEY_SIZE_BYTES] = { 0 };
    uint8_t salt[SALT_SIZE_BYTES] = { 0 };

    // initialize SRTP key and salt with dummy values
    for (int i = 0; i < KEY_SIZE_BYTES; ++i)
        key[i] = i;

    for (int i = 0; i < SALT_SIZE_BYTES; ++i)
        salt[i] = i * 2;

    // Enable SRTP and let user manage the keys
    unsigned flags = RCE_SRTP | RCE_SRTP_KMNGMNT_USER | RCE_SRTP_KEYSIZE_256;

    sender_session = ctx.create_session(RECEIVER_ADDRESS);

    if (sender_session)
    {
        send = sender_session->create_stream(LOCAL_PORT, REMOTE_PORT, RTP_FORMAT_GENERIC, flags);
    }

    EXPECT_NE(nullptr, sender_session);
    EXPECT_NE(nullptr, send);

    if (send)
        send->add_srtp_ctx(key, salt); // add user context

    return std::unique_ptr<std::thread>(new std::thread(receive_func, key, salt));
}

void receive_func(uint8_t key[KEY_SIZE_BYTES], uint8_t salt[SALT_SIZE_BYTES])
{
    /* See sending.cc for more details */
    uvgrtp::context ctx;
    uvgrtp::session* receiver_session = ctx.create_session(SENDER_ADDRESS);

    /* Enable SRTP and let user manage keys */
    unsigned flags = RCE_SRTP | RCE_SRTP_KMNGMNT_USER;

    flags |= RCE_SRTP_KEYSIZE_256;

    /* See sending.cc for more details about create_stream() */
    uvgrtp::media_stream* recv = nullptr;
        
    if (receiver_session)
    {
        recv = receiver_session->create_stream(REMOTE_PORT, LOCAL_PORT, RTP_FORMAT_GENERIC, flags);
    }

    EXPECT_NE(nullptr, receiver_session);
    EXPECT_NE(nullptr, recv);

    if (recv)
    { 
        recv->add_srtp_ctx(key, salt);
    }

    auto start = std::chrono::steady_clock::now();

    uvgrtp::frame::rtp_frame* frame = nullptr;

    if (recv)
    {
        while (std::chrono::steady_clock::now() - start < EXAMPLE_DURATION)
        {
            frame = recv->pull_frame(10);
            if (frame)
            {
                process_frame(frame);
            }
        }
    }

    if (recv)
    {
        receiver_session->destroy_stream(recv);
    }

    if (receiver_session)
    {
        ctx.destroy_session(receiver_session);
    }
}

void process_frame(uvgrtp::frame::rtp_frame* frame)
{
    std::string payload = std::string((char*)frame->payload, frame->payload_len);
    EXPECT_NE(0, frame->payload_len);


    (void)uvgrtp::frame::dealloc_frame(frame);
}