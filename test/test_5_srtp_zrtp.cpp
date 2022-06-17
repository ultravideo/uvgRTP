#include "test_common.hh"


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

    send_packets(RTP_FORMAT_GENERIC, sender_session, send, 10, strlen((char*)"Hello, world!"), 10, true, false);
    cleanup_ms(sender_session, send);

    if (receiver && receiver->joinable())
    {
        receiver->join();
    }

    cleanup_sess(ctx, sender_session);
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
                process_rtp_frame(frame);
            }
        }
    }

    cleanup_ms(receiver_session, recv);
    cleanup_sess(ctx, receiver_session);
}