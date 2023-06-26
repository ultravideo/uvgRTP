#include "test_common.hh"

constexpr char LOCAL_INTERFACE[] = "127.0.0.1";
constexpr uint16_t LOCAL_PORT = 9200;

constexpr char REMOTE_ADDRESS[] = "127.0.0.1";
constexpr uint16_t REMOTE_PORT = 9202;

constexpr uint16_t PAYLOAD_LEN = 256;
constexpr uint16_t FRAME_RATE = 30;
constexpr uint32_t EXAMPLE_RUN_TIME_S = 4;
constexpr int SEND_TEST_PACKETS = FRAME_RATE * EXAMPLE_RUN_TIME_S;
constexpr int PACKET_INTERVAL_MS = 1000 / FRAME_RATE;

void receiver_hook(uvgrtp::frame::rtcp_receiver_report* frame);
void sender_hook(uvgrtp::frame::rtcp_sender_report* frame);
void sdes_hook(uvgrtp::frame::rtcp_sdes_packet* frame);
void app_hook(uvgrtp::frame::rtcp_app_packet* frame);
void cleanup(uvgrtp::context& ctx, uvgrtp::session* local_session, uvgrtp::session* remote_session,
    uvgrtp::media_stream* send, uvgrtp::media_stream* receive);

// Receiver and sender hooks for socket multiplexing test
void m_r_hook1(uvgrtp::frame::rtcp_receiver_report* frame);
void m_r_hook2(uvgrtp::frame::rtcp_receiver_report* frame);
void m_s_hook1(uvgrtp::frame::rtcp_sender_report* frame);
void m_s_hook2(uvgrtp::frame::rtcp_sender_report* frame);

// these are used to check if enough packets were received at the end of a test
static int received1;
static int received2;
static int received3;
static int received4;

TEST(RTCPTests, rtcp) {
    std::cout << "Starting uvgRTP RTCP tests" << std::endl;

    // Creation of RTP stream. See sending example for more details
    uvgrtp::context ctx;
    uvgrtp::session* local_session = ctx.create_session(REMOTE_ADDRESS);
    uvgrtp::session* remote_session = ctx.create_session(LOCAL_INTERFACE);

    int flags = RCE_RTCP;

    // received1 is receiver reports, received2 sender reports
    received1 = 0;
    received2 = 0;

    uvgrtp::media_stream* local_stream = nullptr;
    if (local_session)
    {
        local_stream = local_session->create_stream(LOCAL_PORT, REMOTE_PORT, RTP_FORMAT_GENERIC, flags);
    }

    uvgrtp::media_stream* remote_stream = nullptr;
    if (remote_session)
    {
        remote_stream = remote_session->create_stream(REMOTE_PORT, LOCAL_PORT, RTP_FORMAT_GENERIC, flags);
    }

    EXPECT_NE(nullptr, remote_stream);

    if (local_stream)
    {
        EXPECT_EQ(RTP_OK, local_stream->get_rtcp()->install_receiver_hook(receiver_hook));
        EXPECT_EQ(RTP_OK, local_stream->get_rtcp()->install_sdes_hook(sdes_hook));
    }

    if (remote_stream)
    {
        EXPECT_EQ(RTP_OK, remote_stream->get_rtcp()->install_sender_hook(sender_hook));
        EXPECT_EQ(RTP_OK, remote_stream->get_rtcp()->install_sdes_hook(sdes_hook));
    }

    std::unique_ptr<uint8_t[]> test_frame = std::unique_ptr<uint8_t[]>(new uint8_t[PAYLOAD_LEN]);
    memset(test_frame.get(), 'b', PAYLOAD_LEN);
    send_packets(std::move(test_frame), PAYLOAD_LEN, local_session, local_stream, SEND_TEST_PACKETS, PACKET_INTERVAL_MS, true, RTP_NO_FLAGS);



    cleanup(ctx, local_session, remote_session, local_stream, remote_stream);
    std::cout << "Received RRs: " << received1 << ", received SRs: " << received2 << std::endl;
    EXPECT_TRUE(received1 > 0);
    EXPECT_TRUE(received2 > 0);
}

TEST(RTCPTests, rtcp_app) {
    std::cout << "Starting uvgRTP RTCP tests" << std::endl;

    // Creation of RTP stream. See sending example for more details
    uvgrtp::context ctx;
    uvgrtp::session* local_session = ctx.create_session(REMOTE_ADDRESS);
    uvgrtp::session* remote_session = ctx.create_session(LOCAL_INTERFACE);

    int flags = RCE_RTCP;
    received1 = 0;

    uvgrtp::media_stream* local_stream = nullptr;
    if (local_session)
    {
        local_stream = local_session->create_stream(LOCAL_PORT, REMOTE_PORT, RTP_FORMAT_GENERIC, flags);
        local_stream->configure_ctx(RCC_SESSION_BANDWIDTH, 3000);
    }

    uvgrtp::media_stream* remote_stream = nullptr;
    if (remote_session)
    {
        remote_stream = remote_session->create_stream(REMOTE_PORT, LOCAL_PORT, RTP_FORMAT_GENERIC, flags);
        remote_stream->configure_ctx(RCC_SESSION_BANDWIDTH, 3000);
    }

    EXPECT_NE(nullptr, remote_stream);

    if (local_stream)
    {
        EXPECT_EQ(RTP_OK, local_stream->get_rtcp()->install_receiver_hook(receiver_hook));
        EXPECT_EQ(RTP_OK, local_stream->get_rtcp()->install_sdes_hook(sdes_hook));
        EXPECT_EQ(RTP_OK, local_stream->get_rtcp()->install_app_hook(app_hook));
    }

    if (remote_stream)
    {
        EXPECT_EQ(RTP_OK, remote_stream->get_rtcp()->install_sender_hook(sender_hook));
        EXPECT_EQ(RTP_OK, remote_stream->get_rtcp()->install_sdes_hook(sdes_hook));
        EXPECT_EQ(RTP_OK, remote_stream->get_rtcp()->install_app_hook(app_hook));
    }

    std::unique_ptr<uint8_t[]> test_frame = std::unique_ptr<uint8_t[]>(new uint8_t[PAYLOAD_LEN]);
    memset(test_frame.get(), 'b', PAYLOAD_LEN);
    send_packets(std::move(test_frame), PAYLOAD_LEN, local_session, local_stream, SEND_TEST_PACKETS, PACKET_INTERVAL_MS, true, RTP_NO_FLAGS, true);

    cleanup(ctx, local_session, remote_session, local_stream, remote_stream);

    std::cout << "Received APP packets: " << received1 << std::endl;
    EXPECT_TRUE(received1 > 0);
}

TEST(RTCP_reopen_receiver, rtcp) {
    std::cout << "Starting uvgRTP RTCP reopen receiver test" << std::endl;

    // Creation of RTP stream. See sending example for more details
    uvgrtp::context ctx;
    uvgrtp::session* local_session = ctx.create_session(REMOTE_ADDRESS);
    uvgrtp::session* remote_session = ctx.create_session(LOCAL_INTERFACE);

    int flags = RCE_RTCP;

    received1 = 0;
    received2 = 0;

    uvgrtp::media_stream* local_stream = nullptr;
    if (local_session)
    {
        local_stream = local_session->create_stream(LOCAL_PORT, REMOTE_PORT, RTP_FORMAT_GENERIC, flags);
        local_stream->configure_ctx(RCC_SESSION_BANDWIDTH, 3000);

    }

    uvgrtp::media_stream* remote_stream = nullptr;
    if (remote_session)
    {
        remote_stream = remote_session->create_stream(REMOTE_PORT, LOCAL_PORT, RTP_FORMAT_GENERIC, flags);
        remote_stream->configure_ctx(RCC_SESSION_BANDWIDTH, 3000);

    }

    EXPECT_NE(nullptr, remote_stream);

    if (local_stream)
    {
        EXPECT_EQ(RTP_OK, local_stream->get_rtcp()->install_receiver_hook(receiver_hook));
        EXPECT_EQ(RTP_OK, local_stream->get_rtcp()->install_sdes_hook(sdes_hook));
    }

    if (remote_stream)
    {
        EXPECT_EQ(RTP_OK, remote_stream->get_rtcp()->install_sender_hook(sender_hook));
        EXPECT_EQ(RTP_OK, remote_stream->get_rtcp()->install_sdes_hook(sdes_hook));
    }

    if (local_stream)
    {
        std::unique_ptr<uint8_t[]> test_frame = std::unique_ptr<uint8_t[]>(new uint8_t[PAYLOAD_LEN]);
        memset(test_frame.get(), 'b', PAYLOAD_LEN);
        send_packets(std::move(test_frame), PAYLOAD_LEN, local_session, local_stream, SEND_TEST_PACKETS/2, 
            PACKET_INTERVAL_MS, true, RTP_NO_FLAGS);

        std::cout << "Before reopen, received RRs: " << received1 << ", received SRs: " << received2 << std::endl;
        //EXPECT_TRUE(received1 > 0);
        //EXPECT_TRUE(received2 > 0);
        received1 = 0;
        received2 = 0;

        if (remote_stream)
        {
            std::cout << "Closing and reopening receiver for testing purposes" << std::endl;
            remote_session->destroy_stream(remote_stream);
            remote_stream = remote_session->create_stream(REMOTE_PORT, LOCAL_PORT, RTP_FORMAT_GENERIC, flags);
            remote_stream->configure_ctx(RCC_SESSION_BANDWIDTH, 3000);
            if (remote_stream)
            {
                EXPECT_EQ(RTP_OK, remote_stream->get_rtcp()->install_sender_hook(sender_hook));
                EXPECT_EQ(RTP_OK, remote_stream->get_rtcp()->install_sdes_hook(sdes_hook));
            }
            EXPECT_NE(nullptr, remote_stream);
        }

        test_frame = std::unique_ptr<uint8_t[]>(new uint8_t[PAYLOAD_LEN]);
        memset(test_frame.get(), 'b', PAYLOAD_LEN);
        send_packets(std::move(test_frame), PAYLOAD_LEN, local_session, local_stream, SEND_TEST_PACKETS / 2, 
            PACKET_INTERVAL_MS, true, RTP_NO_FLAGS);

        std::cout << "After reopen, received RRs: " << received1 << ", received SRs: " << received2 << std::endl;
        //EXPECT_TRUE(received1 > 0);
        //EXPECT_TRUE(received2 > 0);
    }

    cleanup(ctx, local_session, remote_session, local_stream, remote_stream);
}

TEST(RTCPTests, rtcp_multiplex)
{
    // Test multiplexing two RTP streams into a single socket with RTCP enabled.
    // RTCP will bind to RTP socket + 1
    std::cout << "Starting RTCP socket multiplexing test: RTP socket + 1" << std::endl;
    uvgrtp::context ctx;
    uvgrtp::session* receiver_sess = ctx.create_session(LOCAL_INTERFACE, REMOTE_ADDRESS);
    uvgrtp::session* sender_sess = ctx.create_session(REMOTE_ADDRESS, LOCAL_INTERFACE);

    uvgrtp::media_stream* sender1 = nullptr;
    uvgrtp::media_stream* receiver1 = nullptr;
    uvgrtp::media_stream* sender2 = nullptr;
    uvgrtp::media_stream* receiver2 = nullptr;
    
    // 1 for RRs, 2 for SRs
    received1 = 0;
    received2 = 0;

    int flags = RCE_FRAGMENT_GENERIC | RCE_RTCP;
    if (sender_sess)
    {
        sender1 = sender_sess->create_stream(LOCAL_PORT, REMOTE_PORT, RTP_FORMAT_GENERIC, flags);
        sender1->configure_ctx(RCC_SSRC, 11);
        sender1->configure_ctx(RCC_REMOTE_SSRC, 22);
        sender2 = sender_sess->create_stream(LOCAL_PORT, REMOTE_PORT, RTP_FORMAT_GENERIC, flags);
        sender2->configure_ctx(RCC_SSRC, 33);
        sender2->configure_ctx(RCC_REMOTE_SSRC, 44);
    }
    if (sender1 && sender2)
    {
        EXPECT_EQ(RTP_OK, sender1->get_rtcp()->install_receiver_hook(m_r_hook1));
        EXPECT_EQ(RTP_OK, sender2->get_rtcp()->install_receiver_hook(m_r_hook2));
    }
    if (receiver_sess)
    {
        receiver1 = receiver_sess->create_stream(REMOTE_PORT, LOCAL_PORT, RTP_FORMAT_GENERIC, flags);
        receiver1->configure_ctx(RCC_SSRC, 22);
        receiver1->configure_ctx(RCC_REMOTE_SSRC, 11);
        receiver2 = receiver_sess->create_stream(REMOTE_PORT, LOCAL_PORT, RTP_FORMAT_GENERIC, flags);
        receiver2->configure_ctx(RCC_SSRC, 44);
        receiver2->configure_ctx(RCC_REMOTE_SSRC, 33);
    }
    if (receiver1 && receiver2)
    {
        EXPECT_EQ(RTP_OK, receiver1->get_rtcp()->install_sender_hook(m_s_hook1));
        EXPECT_EQ(RTP_OK, receiver2->get_rtcp()->install_sender_hook(m_s_hook2));
    }

    int test_packets = 10;
    std::vector<size_t> sizes = { 1000, 2000 };
    for (size_t& size : sizes)
    {
        std::unique_ptr<uint8_t[]> test_frame1 = create_test_packet(RTP_FORMAT_GENERIC, 0, false, size, RTP_NO_FLAGS);
        std::unique_ptr<uint8_t[]> test_frame2 = create_test_packet(RTP_FORMAT_GENERIC, 0, false, size, RTP_NO_FLAGS);
        send_packets(std::move(test_frame1), PAYLOAD_LEN, sender_sess, sender1, SEND_TEST_PACKETS, PACKET_INTERVAL_MS, true, RTP_NO_FLAGS);
        send_packets(std::move(test_frame2), PAYLOAD_LEN, sender_sess, sender2, SEND_TEST_PACKETS, PACKET_INTERVAL_MS, true, RTP_NO_FLAGS);
    }
    std::cout << "Receiver 1 received " << received3 << " sender reports" << std::endl;
    std::cout << "Receiver 2 received " << received4 << " sender reports" << std::endl;
    std::cout << "Sender 1 received " << received1 << " receiver reports" << std::endl;
    std::cout << "Sender 2 received " << received2 << " receiver reports" << std::endl;
    ASSERT_TRUE(received1 > 0);
    ASSERT_TRUE(received2 > 0);
    ASSERT_TRUE(received3 > 0);
    ASSERT_TRUE(received4 > 0);
    cleanup_ms(sender_sess, sender1);
    cleanup_ms(sender_sess, sender2);
    cleanup_ms(receiver_sess, receiver1);
    cleanup_ms(receiver_sess, receiver2);
    cleanup_sess(ctx, sender_sess);
    cleanup_sess(ctx, receiver_sess);

}

TEST(RTCPTests, rtcp_multiplex2)
{
    // Test multiplexing RTCP packets into the same socket as RTP media streams via RCE_RTCP_MUX flag
    // RTCP will bind to RTP socket
    std::cout << "Starting RTCP socket multiplexing test 2: RTP socket" << std::endl;
    uvgrtp::context ctx;
    uvgrtp::session* receiver_sess = ctx.create_session(LOCAL_INTERFACE, REMOTE_ADDRESS);
    uvgrtp::session* sender_sess = ctx.create_session(REMOTE_ADDRESS, LOCAL_INTERFACE);

    uvgrtp::media_stream* sender1 = nullptr;
    uvgrtp::media_stream* receiver1 = nullptr;
    uvgrtp::media_stream* sender2 = nullptr;
    uvgrtp::media_stream* receiver2 = nullptr;

    // 1 for RRs, 2 for SRs
    received1 = 0;
    received2 = 0;
    received3 = 0;
    received4 = 0;

    int flags = RCE_FRAGMENT_GENERIC | RCE_RTCP | RCE_RTCP_MUX;
    if (sender_sess)
    {
        sender1 = sender_sess->create_stream(LOCAL_PORT, REMOTE_PORT, RTP_FORMAT_GENERIC, flags);
        sender1->configure_ctx(RCC_SSRC, 11);
        sender1->configure_ctx(RCC_REMOTE_SSRC, 22);
        sender2 = sender_sess->create_stream(LOCAL_PORT, REMOTE_PORT, RTP_FORMAT_GENERIC, flags);
        sender2->configure_ctx(RCC_SSRC, 33);
        sender2->configure_ctx(RCC_REMOTE_SSRC, 44);
    }
    if (sender1 && sender2)
    {
        EXPECT_EQ(RTP_OK, sender1->get_rtcp()->install_receiver_hook(m_r_hook1));
        EXPECT_EQ(RTP_OK, sender2->get_rtcp()->install_receiver_hook(m_r_hook2));
    }
    if (receiver_sess)
    {
        receiver1 = receiver_sess->create_stream(REMOTE_PORT, LOCAL_PORT, RTP_FORMAT_GENERIC, flags);
        receiver1->configure_ctx(RCC_SSRC, 22);
        receiver1->configure_ctx(RCC_REMOTE_SSRC, 11);
        receiver2 = receiver_sess->create_stream(REMOTE_PORT, LOCAL_PORT, RTP_FORMAT_GENERIC, flags);
        receiver2->configure_ctx(RCC_SSRC, 44);
        receiver2->configure_ctx(RCC_REMOTE_SSRC, 33);
    }
    if (receiver1 && receiver2)
    {
        EXPECT_EQ(RTP_OK, receiver1->get_rtcp()->install_sender_hook(m_s_hook1));
        EXPECT_EQ(RTP_OK, receiver2->get_rtcp()->install_sender_hook(m_s_hook2));
    }

    int test_packets = 10;
    std::vector<size_t> sizes = { 1000, 2000 };
    for (size_t& size : sizes)
    {
        std::unique_ptr<uint8_t[]> test_frame1 = create_test_packet(RTP_FORMAT_GENERIC, 0, false, size, RTP_NO_FLAGS);
        std::unique_ptr<uint8_t[]> test_frame2 = create_test_packet(RTP_FORMAT_GENERIC, 0, false, size, RTP_NO_FLAGS);
        send_packets(std::move(test_frame1), PAYLOAD_LEN, sender_sess, sender1, SEND_TEST_PACKETS, PACKET_INTERVAL_MS, true, RTP_NO_FLAGS);
        send_packets(std::move(test_frame2), PAYLOAD_LEN, sender_sess, sender2, SEND_TEST_PACKETS, PACKET_INTERVAL_MS, true, RTP_NO_FLAGS);
    }
    std::cout << "Receiver 1 received " << received3 << " sender reports" << std::endl;
    std::cout << "Receiver 2 received " << received4 << " sender reports" << std::endl;
    std::cout << "Sender 1 received " << received1 << " receiver reports" << std::endl;
    std::cout << "Sender 2 received " << received2 << " receiver reports" << std::endl;
    ASSERT_TRUE(received1 > 0);
    ASSERT_TRUE(received2 > 0);
    ASSERT_TRUE(received3 > 0);
    ASSERT_TRUE(received4 > 0);
    cleanup_ms(sender_sess, sender1);
    cleanup_ms(sender_sess, sender2);
    cleanup_ms(receiver_sess, receiver1);
    cleanup_ms(receiver_sess, receiver2);
    cleanup_sess(ctx, sender_sess);
    cleanup_sess(ctx, receiver_sess);

}

void m_r_hook1(uvgrtp::frame::rtcp_receiver_report* frame)
{
    //Hook for stream Sender1 ssrc 11 
    std::cout << std::endl << "Sender1 received RTCP receiver report from " << frame->ssrc << std::endl;
    EXPECT_EQ(frame->ssrc, 22);
    ++received1;
    delete frame;
}
void m_r_hook2(uvgrtp::frame::rtcp_receiver_report* frame)
{
    //Hook for stream Sender2 ssrc 33 
    std::cout << std::endl << "Sender2 received RTCP receiver report from " << frame->ssrc << std::endl;
    EXPECT_EQ(frame->ssrc, 44);
    ++received2;
    delete frame;
}
void m_s_hook1(uvgrtp::frame::rtcp_sender_report* frame)
{
    //Hook for stream Receiver1 ssrc 22 
    std::cout << std::endl << "Receiver1 received RTCP sender report from " << frame->ssrc << std::endl;
    EXPECT_EQ(frame->ssrc, 11);
    ++received3;
    delete frame;
}
void m_s_hook2(uvgrtp::frame::rtcp_sender_report* frame)
{
    //Hook for stream Receiver2 ssrc 44 
    std::cout << std::endl << "Receiver2 received RTCP sender report from " << frame->ssrc << std::endl;
    EXPECT_EQ(frame->ssrc, 33);
    ++received4;
    delete frame;
}


void receiver_hook(uvgrtp::frame::rtcp_receiver_report* frame)
{
    ++received1;
    std::cout << std::endl << "RTCP receiver report! ----------" << std::endl;

    for (auto& block : frame->report_blocks)
    {
        std::cout << "ssrc: " << block.ssrc << std::endl;
        std::cout << "fraction: " << uint32_t(block.fraction) << std::endl;
        std::cout << "lost: " << block.lost << std::endl;
        std::cout << "last_seq: " << block.last_seq << std::endl;
        std::cout << "jitter: " << block.jitter << std::endl;
        std::cout << "lsr: " << block.lsr << std::endl;
        std::cout << "dlsr (jiffies): " << block.dlsr << std::endl << std::endl;
    }

    /* RTCP frames can be deallocated using delete */
    delete frame;
}

void sender_hook(uvgrtp::frame::rtcp_sender_report* frame)
{
    ++received2;
    std::cout << std::endl << "RTCP sender report! ----------" << std::endl;
    std::cout << "NTP msw: " << frame->sender_info.ntp_msw << std::endl;
    std::cout << "NTP lsw: " << frame->sender_info.ntp_lsw << std::endl;
    std::cout << "RTP timestamp: " << frame->sender_info.rtp_ts << std::endl;
    std::cout << "packet count: " << frame->sender_info.pkt_cnt << std::endl;
    std::cout << "byte count: " << frame->sender_info.byte_cnt << std::endl;

    for (auto& block : frame->report_blocks)
    {
        std::cout << "ssrc: " << block.ssrc << std::endl;
        std::cout << "fraction: " << uint32_t(block.fraction) << std::endl;
        std::cout << "lost: " << block.lost << std::endl;
        std::cout << "last_seq: " << block.last_seq << std::endl;
        std::cout << "jitter: " << block.jitter << std::endl;
        std::cout << "lsr: " << block.lsr << std::endl;
        std::cout << "dlsr (jiffies): " << block.dlsr << std::endl << std::endl;
    }

    /* RTCP frames can be deallocated using delete */
    delete frame;
}

void sdes_hook(uvgrtp::frame::rtcp_sdes_packet* frame)
{
    std::cout << "Got SDES frame with " << frame->chunks.size() << " chunk" << std::endl;

    for (auto& chunk : frame->chunks)
    {
        for (auto& item : chunk.items)
        {
            if (item.data != nullptr)
            {
                delete[] item.data;
            }
        }
    }

    /* RTCP frames can be deallocated using delete */
    delete frame;
}

void app_hook(uvgrtp::frame::rtcp_app_packet* frame)
{
    ++received1;
    size_t payload_len = size_t(frame->header.length - 2)*4;
    std::string payload = std::string((char*)frame->payload, payload_len);
    std::string name = std::string((char*)frame->name, 4);
    uint8_t subtype = uint8_t(frame->header.pkt_subtype);

    std::cout << std::endl << "APP frame! ----------" << std::endl;
    std::cout << "ssrc: " << frame->ssrc << std::endl;
    std::cout << "Name: " << name << " and content: " << payload << std::endl;
    std::cout << "Calculated payload length " << payload_len << std::endl;
    std::cout << "Payload length field "      << frame->payload_len << std::endl;

    EXPECT_EQ(name, "Test");
    EXPECT_EQ(payload, "ABCD");
    EXPECT_EQ(payload_len, 4);
    EXPECT_EQ(subtype, 1);

    if (payload_len > 0)
    {
        delete[] frame->payload;
    }
    delete frame;
}

void cleanup(uvgrtp::context& ctx, uvgrtp::session* local_session, uvgrtp::session* remote_session,
    uvgrtp::media_stream* send, uvgrtp::media_stream* receive)
{
    cleanup_ms(local_session, send);
    if (receive) {
        cleanup_ms(remote_session, receive);
    }
    cleanup_sess(ctx, local_session);
    cleanup_sess(ctx, remote_session);
}