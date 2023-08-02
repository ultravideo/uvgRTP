#include "test_common.hh"


// network parameters of example
constexpr char SENDER_ADDRESS[] = "127.0.0.1";
constexpr uint16_t SENDER_PORT = 9000;

constexpr char RECEIVER_ADDRESS[] = "127.0.0.1";
constexpr uint16_t RECEIVER_PORT = 9042;

constexpr auto EXAMPLE_DURATION_S = std::chrono::seconds(2);

// encryption parameters of example
enum Key_length { SRTP_128 = 128, SRTP_196 = 196, SRTP_256 = 256 };
constexpr int SALT_SIZE = 112;
constexpr int SALT_SIZE_BYTES = SALT_SIZE / 8;

void user_send_func(uint8_t *key, uint8_t salt[SALT_SIZE_BYTES], uint8_t key_size);
void user_receive_func(uint8_t *key, uint8_t salt[SALT_SIZE_BYTES], uint8_t key_size);
void zrtp_sender_func(uvgrtp::session* sender_session, int sender_port, int receiver_port, unsigned int flags, bool mux);
void zrtp_receive_func(uvgrtp::session* receiver_session, int sender_port, int receiver_port, unsigned int flags, bool mux);

void test_user_key(Key_length len);
int received_packets;
// User key management test

TEST(EncryptionTests, srtp_user_key_128)
{
    test_user_key(SRTP_128);
}

TEST(EncryptionTests, srtp_user_key_196)
{
    test_user_key(SRTP_196);
}

TEST(EncryptionTests, srtp_user_key_256)
{
    test_user_key(SRTP_256);
}

void test_user_key(Key_length len)
{
    std::cout << "Starting ZRTP sender thread" << std::endl;
    uvgrtp::context ctx;

    if (!ctx.crypto_enabled())
    {
        std::cout << "Please link crypto to uvgRTP library in order to tests its SRTP user keys!" << std::endl;
        FAIL();
        return;
    }

    uint8_t *key = new uint8_t[len];
    uint8_t salt[SALT_SIZE_BYTES] = { 0 };

    // initialize SRTP key and salt with dummy values
    for (int i = 0; i < len; ++i)
        key[i] = i;

    for (int i = 0; i < SALT_SIZE_BYTES; ++i)
        salt[i] = i * 2;

    std::unique_ptr<std::thread> sender_thread = std::unique_ptr<std::thread>(new std::thread(user_send_func, key, salt, len));
    std::unique_ptr<std::thread> receiver_thread = std::unique_ptr<std::thread>(new std::thread(user_receive_func, key, salt, len));

    if (sender_thread && sender_thread->joinable())
    {
        sender_thread->join();
    }

    if (receiver_thread && receiver_thread->joinable())
    {
        receiver_thread->join();
    }

    delete[] key;
}

void user_send_func(uint8_t* key, uint8_t salt[SALT_SIZE_BYTES], uint8_t key_size)
{
    uvgrtp::context ctx;
    uvgrtp::session* sender_session = nullptr;
    uvgrtp::media_stream* send = nullptr;

    sender_session = ctx.create_session(RECEIVER_ADDRESS);

    // Enable SRTP and let user manage the keys
    unsigned flags = RCE_SRTP | RCE_SRTP_KMNGMNT_USER;
    if (key_size == 192)
    {
        flags |= RCE_SRTP_KEYSIZE_192;
    }
    else if (key_size == 256)
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
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
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

void user_receive_func(uint8_t *key, uint8_t salt[SALT_SIZE_BYTES], uint8_t key_size)
{
    /* See sending.cc for more details */
    uvgrtp::context ctx;
    uvgrtp::session* receiver_session = ctx.create_session(SENDER_ADDRESS);

    /* Enable SRTP and let user manage keys */
    unsigned flags = RCE_SRTP | RCE_SRTP_KMNGMNT_USER;
    received_packets = 0;

    if (key_size == 192)
    {
        flags |= RCE_SRTP_KEYSIZE_192;
    }
    else if (key_size == 256)
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
                std::cout << "Received frame" << std::endl;
                ++received_packets;
                process_rtp_frame(frame);
            }
        }
    }
    std::cout << received_packets << " / 10 packets received" << std::endl;
    EXPECT_TRUE(received_packets > 5);

    cleanup_ms(receiver_session, recv);
    cleanup_sess(ctx, receiver_session);
}


// ZRTP key management tests

TEST(EncryptionTests, zrtp)
{
    uvgrtp::context ctx;

    if (!ctx.crypto_enabled())
    {
        std::cout << "Please link crypto to uvgRTP library in order to tests its ZRTP feature!" << std::endl;
        FAIL();
        return;
    }
    
    uvgrtp::session* sender_session = ctx.create_session(RECEIVER_ADDRESS, SENDER_ADDRESS);
    uvgrtp::session* receiver_session = ctx.create_session(SENDER_ADDRESS, RECEIVER_ADDRESS);

    unsigned zrtp_flags = RCE_SRTP | RCE_SRTP_KMNGMNT_ZRTP;
    received_packets = 0;

    std::unique_ptr<std::thread> sender_thread =
        std::unique_ptr<std::thread>(new std::thread(zrtp_sender_func, sender_session, SENDER_PORT, RECEIVER_PORT, zrtp_flags, false));

    std::unique_ptr<std::thread> receiver_thread =
        std::unique_ptr<std::thread>(new std::thread(zrtp_receive_func, receiver_session, SENDER_PORT, RECEIVER_PORT, zrtp_flags, false));

    if (sender_thread && sender_thread->joinable())
    {
        sender_thread->join();
    }

    if (receiver_thread && receiver_thread->joinable())
    {
        receiver_thread->join();
    }

    std::cout << received_packets << " / 10 packets received" << std::endl;
    EXPECT_TRUE(received_packets > 5);

    cleanup_sess(ctx, sender_session);
    cleanup_sess(ctx, receiver_session);
}

TEST(EncryptionTests, zrtp_authenticate)
{
    uvgrtp::context ctx;

    if (!ctx.crypto_enabled())
    {
        std::cout << "Please link crypto to uvgRTP library in order to tests its ZRTP feature!" << std::endl;
        FAIL();
        return;
    }

    uvgrtp::session* sender_session = ctx.create_session(RECEIVER_ADDRESS, SENDER_ADDRESS);
    uvgrtp::session* receiver_session = ctx.create_session(SENDER_ADDRESS, RECEIVER_ADDRESS);

    unsigned zrtp_flags = RCE_SRTP | RCE_SRTP_KMNGMNT_ZRTP | RCE_SRTP_AUTHENTICATE_RTP;
    received_packets = 0;

    std::unique_ptr<std::thread> sender_thread =
        std::unique_ptr<std::thread>(new std::thread(zrtp_sender_func, sender_session, SENDER_PORT, RECEIVER_PORT, zrtp_flags, false));

    std::unique_ptr<std::thread> receiver_thread =
        std::unique_ptr<std::thread>(new std::thread(zrtp_receive_func, receiver_session, SENDER_PORT, RECEIVER_PORT, zrtp_flags, false));

    if (sender_thread && sender_thread->joinable())
    {
        sender_thread->join();
    }

    if (receiver_thread && receiver_thread->joinable())
    {
        receiver_thread->join();
    }

    std::cout << received_packets << " / 10 packets received" << std::endl;
    EXPECT_TRUE(received_packets > 5);

    cleanup_sess(ctx, sender_session);
    cleanup_sess(ctx, receiver_session);
}

TEST(EncryptionTests, zrtp_multistream)
{
    uvgrtp::context ctx;
    received_packets = 0;

    if (!ctx.crypto_enabled())
    {
        std::cout << "Please link crypto to uvgRTP library in order to tests its ZRTP feature!" << std::endl;
        FAIL();
        return;
    }

    /* Enable SRTP and ZRTP */
    unsigned zrtp_dh_flags      = RCE_SRTP   | RCE_SRTP_KMNGMNT_ZRTP | RCE_ZRTP_DIFFIE_HELLMAN_MODE;

    // only one of the streams should perform DH
    unsigned int zrtp_multistream_flags = RCE_SRTP | RCE_SRTP_KMNGMNT_ZRTP | RCE_ZRTP_MULTISTREAM_MODE;

    uvgrtp::session* sender_session = ctx.create_session(RECEIVER_ADDRESS, SENDER_ADDRESS);
    uvgrtp::session* receiver_session = ctx.create_session(SENDER_ADDRESS, RECEIVER_ADDRESS);

    std::unique_ptr<std::thread> sender_thread1 = 
        std::unique_ptr<std::thread>(new std::thread(zrtp_sender_func, sender_session, SENDER_PORT + 2, RECEIVER_PORT + 2, zrtp_dh_flags, false));

    std::unique_ptr<std::thread> receiver_thread1 =
        std::unique_ptr<std::thread>(new std::thread(zrtp_receive_func, receiver_session, SENDER_PORT + 2, RECEIVER_PORT + 2, zrtp_dh_flags, false));

    std::unique_ptr<std::thread> sender_thread2 = 
        std::unique_ptr<std::thread>(new std::thread(zrtp_sender_func, sender_session, SENDER_PORT + 4, RECEIVER_PORT + 4, zrtp_multistream_flags, false));

    std::unique_ptr<std::thread> receiver_thread2 = 
        std::unique_ptr<std::thread>(new std::thread(zrtp_receive_func, receiver_session, SENDER_PORT + 4, RECEIVER_PORT + 4, zrtp_multistream_flags, false));

    std::unique_ptr<std::thread> sender_thread3 =
        std::unique_ptr<std::thread>(new std::thread(zrtp_sender_func, sender_session, SENDER_PORT + 6, RECEIVER_PORT + 6, zrtp_multistream_flags, false));

    std::unique_ptr<std::thread> receiver_thread3 =
        std::unique_ptr<std::thread>(new std::thread(zrtp_receive_func, receiver_session, SENDER_PORT + 6, RECEIVER_PORT + 6, zrtp_multistream_flags, false));

    if (receiver_thread1 && receiver_thread1->joinable())
    {
        receiver_thread1->join();
    }

    if (sender_thread1 && sender_thread1->joinable())
    {
        sender_thread1->join();
    }

    if (sender_thread2 && sender_thread2->joinable())
    {
        sender_thread2->join();
    }

    if (receiver_thread2 && receiver_thread2->joinable())
    {
        receiver_thread2->join();
    }

    if (sender_thread3 && sender_thread3->joinable())
    {
        sender_thread3->join();
    }

    if (receiver_thread3 && receiver_thread3->joinable())
    {
        receiver_thread3->join();
    }

    std::cout << received_packets << " / 30 packets received" << std::endl;
    EXPECT_TRUE(received_packets > 25);

    cleanup_sess(ctx, sender_session);
    cleanup_sess(ctx, receiver_session);
}

TEST(EncryptionTests, zrtp_multistream_mux)
{
    std::cout << "Testing ZRTP multiple streams in a single socket" << std::endl;

    uvgrtp::context send_ctx;
    uvgrtp::context recv_ctx;
    received_packets = 0;

    if (!send_ctx.crypto_enabled())
    {
        std::cout << "Please link crypto to uvgRTP library in order to tests its ZRTP feature!" << std::endl;
        FAIL();
        return;
    }

    /* Enable SRTP and ZRTP */
    unsigned zrtp_dh_flags = RCE_SRTP | RCE_ZRTP_DIFFIE_HELLMAN_MODE;

    // only one of the streams should perform DH
    unsigned int zrtp_multistream_flags = RCE_SRTP | RCE_ZRTP_MULTISTREAM_MODE;

    uvgrtp::session* sender_session = send_ctx.create_session(RECEIVER_ADDRESS, SENDER_ADDRESS);
    uvgrtp::session* receiver_session = recv_ctx.create_session(SENDER_ADDRESS, RECEIVER_ADDRESS);

    std::unique_ptr<std::thread> sender_thread1 =
        std::unique_ptr<std::thread>(new std::thread(zrtp_sender_func, sender_session, SENDER_PORT, RECEIVER_PORT, zrtp_dh_flags, true));

    std::unique_ptr<std::thread> receiver_thread1 =
        std::unique_ptr<std::thread>(new std::thread(zrtp_receive_func, receiver_session, SENDER_PORT, RECEIVER_PORT, zrtp_dh_flags, true));

    //std::this_thread::sleep_for(std::chrono::milliseconds(100));

    std::unique_ptr<std::thread> sender_thread2 =
        std::unique_ptr<std::thread>(new std::thread(zrtp_sender_func, sender_session, SENDER_PORT, RECEIVER_PORT, zrtp_multistream_flags, true));

    std::unique_ptr<std::thread> receiver_thread2 =
        std::unique_ptr<std::thread>(new std::thread(zrtp_receive_func, receiver_session, SENDER_PORT, RECEIVER_PORT, zrtp_multistream_flags, true));

    if (receiver_thread1 && receiver_thread1->joinable())
    {
        receiver_thread1->join();
    }

    if (sender_thread1 && sender_thread1->joinable())
    {
        sender_thread1->join();
    }

    if (sender_thread2 && sender_thread2->joinable())
    {
        sender_thread2->join();
    }

    if (receiver_thread2 && receiver_thread2->joinable())
    {
        receiver_thread2->join();
    }

    std::cout << received_packets << " / 20 packets received" << std::endl;
    EXPECT_TRUE(received_packets > 15);

    cleanup_sess(send_ctx, sender_session);
    cleanup_sess(recv_ctx, receiver_session);
}

void zrtp_sender_func(uvgrtp::session* sender_session, int sender_port, int receiver_port, unsigned int flags, bool mux)
{
    std::cout << "Starting ZRTP sender thread" << std::endl;

    /* See sending.cc for more details about create_stream() */
    uvgrtp::media_stream* send = nullptr;
    send = sender_session->create_stream(sender_port, receiver_port, RTP_FORMAT_GENERIC, flags);

    if (sender_session && mux)
    {
        if (flags & RCE_ZRTP_MULTISTREAM_MODE) {
            send->configure_ctx(RCC_REMOTE_SSRC, 11);
            send->configure_ctx(RCC_SSRC, 22);
        }
        else {
            send->configure_ctx(RCC_REMOTE_SSRC, 33);
            send->configure_ctx(RCC_SSRC, 44);
        }
        send->start_zrtp();
        //send = sender_session->create_stream(sender_port, receiver_port, RTP_FORMAT_GENERIC, flags);
    }
    //Sleep for a bit so that the receiver is ready to receives
    std::this_thread::sleep_for(std::chrono::milliseconds(50)); 
    auto start = std::chrono::steady_clock::now();

    uvgrtp::frame::rtp_frame* frame = nullptr;

    if (send)
    {
        int test_packets = 10;
        size_t packet_size = 1000;
        int packet_interval_ms = EXAMPLE_DURATION_S.count() * 1000 / test_packets;

        std::unique_ptr<uint8_t[]> test_frame = create_test_packet(RTP_FORMAT_GENERIC, 0, false, packet_size, RTP_NO_FLAGS);
        send_packets(std::move(test_frame), packet_size, sender_session, send, test_packets, packet_interval_ms, false, RTP_NO_FLAGS);
    }
    cleanup_ms(sender_session, send);
}

void zrtp_receive_func(uvgrtp::session* receiver_session, int sender_port, int receiver_port, unsigned int flags, bool mux)
{
    std::cout << "Starting ZRTP receiver thread" << std::endl;

    /* See sending.cc for more details about create_stream() */
    uvgrtp::media_stream* recv = nullptr;
    recv = receiver_session->create_stream(receiver_port, sender_port, RTP_FORMAT_GENERIC, flags);

    if (receiver_session && mux)
    {
        if (flags & RCE_ZRTP_MULTISTREAM_MODE) {
            recv->configure_ctx(RCC_REMOTE_SSRC, 22);
            recv->configure_ctx(RCC_SSRC, 11);
        }
        else {
            recv->configure_ctx(RCC_REMOTE_SSRC, 44);
            recv->configure_ctx(RCC_SSRC, 33);
        }
        recv->start_zrtp();
        //recv = receiver_session->create_stream(receiver_port, sender_port, RTP_FORMAT_GENERIC, flags);
    }

    auto start = std::chrono::steady_clock::now();

    uvgrtp::frame::rtp_frame* frame = nullptr;

    if (recv)
    {
        while (std::chrono::steady_clock::now() - start < std::chrono::seconds(4))
        {
            frame = recv->pull_frame(10);
            if (frame)
            {
                std::cout << "Stream " << flags << " received frame" << std::endl;
                ++received_packets;
                process_rtp_frame(frame);
            }
        }
    }

    cleanup_ms(receiver_session, recv);
}