#include "test_common.hh"


// network parameters of example
constexpr char SENDER_ADDRESS[] = "127.0.0.1";
constexpr uint16_t SENDER_PORT = 9000;

constexpr char RECEIVER_ADDRESS[] = "127.0.0.1";
constexpr uint16_t RECEIVER_PORT = 9002;

constexpr auto EXAMPLE_DURATION_S = std::chrono::seconds(1);

// encryption parameters of example
enum Key_length { SRTP_128 = 128, SRTP_196 = 196, SRTP_256 = 256 };
constexpr Key_length KEY_SIZE = SRTP_128; // change this to change tested key length
constexpr int KEY_SIZE_BYTES = KEY_SIZE / 8;
constexpr int SALT_SIZE = 112;
constexpr int SALT_SIZE_BYTES = SALT_SIZE / 8;

void user_send_func(uint8_t key[KEY_SIZE_BYTES], uint8_t salt[SALT_SIZE_BYTES]);
void user_receive_func(uint8_t key[KEY_SIZE_BYTES], uint8_t salt[SALT_SIZE_BYTES]);
void zrtp_sender_func();
void zrtp_receive_func();


// User key management test

TEST(EncryptionTests, srtp_user_keys)
{
    std::cout << "Starting ZRTP sender thread" << std::endl;
    uvgrtp::context ctx;

    if (!ctx.crypto_enabled())
    {
        std::cout << "Please link crypto to uvgRTP library in order to tests its SRTP user keys!" << std::endl;
        FAIL();
        return;
    }

    uint8_t key[KEY_SIZE_BYTES] = { 0 };
    uint8_t salt[SALT_SIZE_BYTES] = { 0 };

    // initialize SRTP key and salt with dummy values
    for (int i = 0; i < KEY_SIZE_BYTES; ++i)
        key[i] = i;

    for (int i = 0; i < SALT_SIZE_BYTES; ++i)
        salt[i] = i * 2;

    std::unique_ptr<std::thread> sender_thread = std::unique_ptr<std::thread>(new std::thread(user_send_func, key, salt));
    std::unique_ptr<std::thread> receiver_thread = std::unique_ptr<std::thread>(new std::thread(user_receive_func, key, salt));

    if (sender_thread && sender_thread->joinable())
    {
        sender_thread->join();
    }

    if (receiver_thread && receiver_thread->joinable())
    {
        receiver_thread->join();
    }
}

void user_send_func(uint8_t key[KEY_SIZE_BYTES], uint8_t salt[SALT_SIZE_BYTES])
{
    uvgrtp::context ctx;
    uvgrtp::session* sender_session = nullptr;
    uvgrtp::media_stream* send = nullptr;

    sender_session = ctx.create_session(RECEIVER_ADDRESS);

    // Enable SRTP and let user manage the keys
    unsigned flags = RCE_SRTP | RCE_SRTP_KMNGMNT_USER;
    if (KEY_SIZE == 192)
    {
        flags |= RCE_SRTP_KEYSIZE_192;
    }
    else if (KEY_SIZE == 256)
    {
        flags |= RCE_SRTP_KEYSIZE_256;
    }

    if (sender_session)
    {
        send = sender_session->create_stream(SENDER_PORT, RECEIVER_PORT, RTP_FORMAT_GENERIC, flags);
    }

    if (send)
        send->add_srtp_ctx(key, salt); // add user context

    EXPECT_NE(nullptr, sender_session);
    EXPECT_NE(nullptr, send);

    if (sender_session && send)
    {
        int test_packets = 10;
        size_t frame_size = strlen((char*)"Hello, world!");
        std::unique_ptr<uint8_t[]> test_frame = create_test_packet(RTP_FORMAT_GENERIC, 0, false, frame_size, RTP_NO_FLAGS);
        send_packets(std::move(test_frame), frame_size, sender_session, send, test_packets, 0, true, RTP_NO_FLAGS);

        cleanup_ms(sender_session, send);
        cleanup_sess(ctx, sender_session);
    }
}

void user_receive_func(uint8_t key[KEY_SIZE_BYTES], uint8_t salt[SALT_SIZE_BYTES])
{
    /* See sending.cc for more details */
    uvgrtp::context ctx;
    uvgrtp::session* receiver_session = ctx.create_session(SENDER_ADDRESS);

    /* Enable SRTP and let user manage keys */
    unsigned flags = RCE_SRTP | RCE_SRTP_KMNGMNT_USER;

    if (KEY_SIZE == 192)
    {
        flags |= RCE_SRTP_KEYSIZE_192;
    }
    else if (KEY_SIZE == 256)
    {
        flags |= RCE_SRTP_KEYSIZE_256;
    }

    /* See sending.cc for more details about create_stream() */
    uvgrtp::media_stream* recv = nullptr;

    if (receiver_session)
    {
        recv = receiver_session->create_stream(RECEIVER_PORT, SENDER_PORT, RTP_FORMAT_GENERIC, flags);
    }

    if (recv)
    {
        recv->add_srtp_ctx(key, salt);
    }

    auto start = std::chrono::steady_clock::now();

    uvgrtp::frame::rtp_frame* frame = nullptr;

    if (recv)
    {
        while (std::chrono::steady_clock::now() - start < EXAMPLE_DURATION_S)
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


// ZRTP key management test

TEST(EncryptionTests, zrtp)
{
    uvgrtp::context ctx;

    if (!ctx.crypto_enabled())
    {
        std::cout << "Please link crypto to uvgRTP library in order to tests its ZRTP feature!" << std::endl;
        FAIL();
        return;
    }

    std::unique_ptr<std::thread> sender_thread = std::unique_ptr<std::thread>(new std::thread(zrtp_sender_func));
    std::unique_ptr<std::thread> receiver_thread = std::unique_ptr<std::thread>(new std::thread(zrtp_receive_func));

    if (sender_thread && sender_thread->joinable())
    {
        sender_thread->join();
    }

    if (receiver_thread && receiver_thread->joinable())
    {
        receiver_thread->join();
    }
}

void zrtp_sender_func()
{
    std::cout << "Starting ZRTP sender thread" << std::endl;
    /* See sending.cc for more details */
    uvgrtp::context ctx;
    uvgrtp::session* sender_session = ctx.create_session(RECEIVER_ADDRESS, SENDER_ADDRESS);

    /* Enable SRTP and ZRTP */
    unsigned flags = RCE_SRTP | RCE_SRTP_KMNGMNT_ZRTP;

    /* See sending.cc for more details about create_stream() */
    uvgrtp::media_stream* send = nullptr;

    if (sender_session)
    {
        send = sender_session->create_stream(SENDER_PORT, RECEIVER_PORT, RTP_FORMAT_GENERIC, flags);
    }

    auto start = std::chrono::steady_clock::now();

    uvgrtp::frame::rtp_frame* frame = nullptr;

    if (send)
    {
        int test_packets = 10;
        size_t packet_size = 1000;

        std::unique_ptr<uint8_t[]> test_frame = create_test_packet(RTP_FORMAT_GENERIC, 0, false, packet_size, RTP_NO_FLAGS);
        send_packets(std::move(test_frame), packet_size, sender_session, send, test_packets, EXAMPLE_DURATION_S.count()*1000, true, RTP_NO_FLAGS);
    }

    cleanup_ms(sender_session, send);
    cleanup_sess(ctx, sender_session);
}

void zrtp_receive_func()
{
    std::cout << "Starting ZRTP receiver thread" << std::endl;
    /* See sending.cc for more details */
    uvgrtp::context ctx;
    uvgrtp::session* receiver_session = ctx.create_session(SENDER_ADDRESS, RECEIVER_ADDRESS);

    /* Enable SRTP and let user manage keys */
    unsigned flags = RCE_SRTP | RCE_SRTP_KMNGMNT_ZRTP;

    /* See sending.cc for more details about create_stream() */
    uvgrtp::media_stream* recv = nullptr;

    if (receiver_session)
    {
        recv = receiver_session->create_stream(RECEIVER_PORT, SENDER_PORT, RTP_FORMAT_GENERIC, flags);
    }

    auto start = std::chrono::steady_clock::now();

    uvgrtp::frame::rtp_frame* frame = nullptr;

    if (recv)
    {
        while (std::chrono::steady_clock::now() - start < EXAMPLE_DURATION_S)
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