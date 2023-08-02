#pragma once

#include <gtest/gtest.h>
#include "uvgrtp/lib.hh"

class Test_receiver;

void wait_until_next_frame(std::chrono::high_resolution_clock::time_point& start,
    int frame_index, int packet_interval_ms);

inline std::unique_ptr<uint8_t[]> create_test_packet(rtp_format_t format, uint8_t nal_type, 
    bool add_start_code, size_t size, int rtp_flags);

inline void test_packet_size(std::unique_ptr<uint8_t[]> test_packet, int packets, size_t size,
    uvgrtp::session* sess, uvgrtp::media_stream* sender, uvgrtp::media_stream* receiver, int rtp_flags, int framerate = 25);

inline void test_packet_size(std::unique_ptr<uint8_t[]> test_packet, int packets, size_t size,
    uvgrtp::session* sess,
    uvgrtp::media_stream* sender, std::vector<uvgrtp::media_stream*> const& receiver,
    int rtp_flags, int framerate = 25);

inline void send_packets(std::unique_ptr<uint8_t[]> test_packet, size_t size, 
    uvgrtp::session* sess, uvgrtp::media_stream* sender,
    int packets, int packet_interval_ms, bool print_progress, int rtp_flags, bool send_app = false, bool user = false);

inline void add_hook(Test_receiver* tester, uvgrtp::media_stream* receiver, 
    void (*hook)(void*, uvgrtp::frame::rtp_frame*));

inline void cleanup_sess(uvgrtp::context& ctx, uvgrtp::session* sess);
inline void cleanup_ms(uvgrtp::session* sess, uvgrtp::media_stream* ms);

inline void process_rtp_frame(uvgrtp::frame::rtp_frame* frame);
inline void rtp_receive_hook(void* arg, uvgrtp::frame::rtp_frame* frame);

inline void set_nal_unit(uint8_t* frame, size_t& pos, bool zero_prefix, uint8_t zeros,
    uint8_t first_byte, uint8_t second_byte);
inline void set_nal_unit(uint8_t* frame, size_t& pos, bool zero_prefix, uint8_t zeros,
    uint8_t first_byte);

class Test_receiver
{
public:
    Test_receiver(int expectedPackets) :
        receivedPackets_(0),
        expectedPackets_(expectedPackets)
    {}

    void receive()
    {
        ++receivedPackets_;
    }

    void gotAll()
    {
        EXPECT_EQ(receivedPackets_, expectedPackets_);
    }

private:

    int receivedPackets_;
    int expectedPackets_;
};

inline std::unique_ptr<uint8_t[]> create_test_packet(rtp_format_t format, uint8_t nal_type,
    bool add_start_code, size_t size, int rtp_flags)
{
    std::unique_ptr<uint8_t[]> test_frame = std::unique_ptr<uint8_t[]>(new uint8_t[size]);
    memset(test_frame.get(), 'b', size);

    if (add_start_code && (size >= 6 || (size >= 4 && format == RTP_FORMAT_H264)))
    {
        size_t pos = 0;
        bool zero_prefix = !(rtp_flags & RTP_NO_H26X_SCL);

        if (format == RTP_FORMAT_H264)
        {
            // https://datatracker.ietf.org/doc/html/rfc6184#section-1.3
            set_nal_unit(test_frame.get(), pos, zero_prefix, 2, nal_type);
        }
        else if (format == RTP_FORMAT_H265)
        {
            // see https://datatracker.ietf.org/doc/html/rfc7798#section-1.1.4
            set_nal_unit(test_frame.get(), pos, zero_prefix, 3, (nal_type << 1), 0);
        }
        else if (format == RTP_FORMAT_H266)
        {
            // see https://datatracker.ietf.org/doc/html/draft-ietf-avtcore-rtp-vvc#section-1.1.4
            set_nal_unit(test_frame.get(), pos, zero_prefix, 3, 0, (nal_type << 3));
        }
    }

    return test_frame;
}

inline void send_packets(std::unique_ptr<uint8_t[]> test_packet, size_t size, 
    uvgrtp::session* sess, uvgrtp::media_stream* sender,
    int packets, int packet_interval_ms, bool print_progress, int rtp_flags, bool send_app, bool user)
{
    EXPECT_NE(nullptr, sess);
    EXPECT_NE(nullptr, sender);
    if (sess && sender)
    {
        std::cout << "Sending " << packets << " test packets with size " << size 
            << " and interval " << packet_interval_ms << "ms" << std::endl;
        
        std::chrono::high_resolution_clock::time_point start = std::chrono::high_resolution_clock::now();
        for (unsigned int i = 0; i < packets; ++i)
        {
            if (i % 60 == 0 && send_app)
            {
                const char* data = "ABCD";
                sender->get_rtcp()->send_app_packet("Test", 1, 4, (uint8_t*)data);
            }
            /* User packets disabled for now
            if (i % 4 == 0 && user) {
                uint8_t data[5] = {20, 25, 30, 35, 40};
                uint8_t* ptr = &data[0];
                sender->push_user_packet(ptr, 5);
            }*/

            rtp_error_t ret = RTP_OK;

            if (rtp_flags & RTP_COPY)
            {
                uint8_t* test_frame = new uint8_t[size];
                memcpy(test_frame, test_packet.get(), size);

                if ((ret = sender->push_frame(std::move(test_frame), size, rtp_flags)) != RTP_OK)
                {
                    std::cout << "Failed to send test packet! Return value: " << ret << std::endl;
                    return;
                }

                delete[] test_frame; // copying leaves the responsibility of deletion to us
            }
            else
            {
                std::unique_ptr<uint8_t[]> test_frame = std::unique_ptr<uint8_t[]>(new uint8_t[size]);
                memcpy(test_frame.get(), test_packet.get(), size);

                if ((ret = sender->push_frame(std::move(test_frame), size, rtp_flags)) != RTP_OK)
                {
                    std::cout << "Failed to send test packet! Return value: " << ret << std::endl;
                    return;
                }
            }

            if (print_progress && packets >= 10 && i % (packets / 10) == packets / 10 - 1)
            {
                std::cout << "Sent " << (i + 1) * 100 / packets << " % of data" << std::endl;
            }

            if (packet_interval_ms > 0)
            {
                wait_until_next_frame(start, i, packet_interval_ms);
            }
        }
    }
}

inline void wait_until_next_frame(std::chrono::high_resolution_clock::time_point& start, int frame_index, int packet_interval_ms)
{
    // wait until it is time to send the next frame. Simulates a steady sending pace
    // and included only for demostration purposes since you can use uvgRTP to send
    // packets as fast as desired
    auto time_since_start = std::chrono::high_resolution_clock::now() - start;
    auto next_frame_slot = (frame_index + 1) * std::chrono::milliseconds(packet_interval_ms);

    if (next_frame_slot > time_since_start)
    {
        std::this_thread::sleep_for(next_frame_slot - time_since_start);
    }
}

inline void cleanup_sess(uvgrtp::context& ctx, uvgrtp::session* sess)
{
    EXPECT_NE(nullptr, sess);
    if (sess)
    {
        // Session must be destroyed manually
        ctx.destroy_session(sess);
    }
}

inline void cleanup_ms(uvgrtp::session* sess, uvgrtp::media_stream* ms)
{
    EXPECT_NE(nullptr, ms);
    EXPECT_NE(nullptr, sess);
    if (sess && ms)
    {
        sess->destroy_stream(ms);
    }
}

inline void test_packet_size(std::unique_ptr<uint8_t[]> test_packet, int packets, size_t size, 
    uvgrtp::session* sess, uvgrtp::media_stream* sender, uvgrtp::media_stream* receiver, int rtp_flags, int framerate)
{
    EXPECT_NE(nullptr, sess);
    EXPECT_NE(nullptr, sender);
    EXPECT_NE(nullptr, receiver);

    if (sess && sender && receiver)
    {
        Test_receiver* tester = new Test_receiver(packets);

        int interval_ms = 1000/framerate;

        add_hook(tester, receiver, rtp_receive_hook);

        // to increase the likelyhood that receiver thread is ready to receive
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        send_packets(std::move(test_packet), size, sess, sender, packets, interval_ms, false, rtp_flags);

        std::this_thread::sleep_for(std::chrono::milliseconds(100 + size/500));

        tester->gotAll();
        delete tester;
    }
}

inline void test_packet_size(std::unique_ptr<uint8_t[]> test_packet, int packets, size_t size,
    uvgrtp::session* sess, uvgrtp::media_stream* sender,
    std::vector<uvgrtp::media_stream*> const& receivers,
    int rtp_flags, int framerate)
{
    EXPECT_NE(nullptr, sess);
    EXPECT_NE(nullptr, sender);

    if (sess && sender)
    {
        std::vector<Test_receiver> testers(receivers.size(), { packets });

        int interval_ms = 1000 / framerate;

        for (auto i = 0; i < receivers.size(); ++i) {
            auto receiver = receivers[i];

            EXPECT_NE(nullptr, receiver);

            if (!receiver) return;

            add_hook(&testers[i], receiver, rtp_receive_hook);
        }

        // to increase the likelyhood that receiver thread is ready to receive
        std::this_thread::sleep_for(std::chrono::milliseconds(25));

        send_packets(std::move(test_packet), size, sess, sender, packets, interval_ms, false, rtp_flags);

        std::this_thread::sleep_for(std::chrono::milliseconds(50 + size / 500));

        for (auto& tester : testers) tester.gotAll();
    }
}

inline void add_hook(Test_receiver* tester, uvgrtp::media_stream* receiver,
    void (*hook)(void*, uvgrtp::frame::rtp_frame*))
{
    EXPECT_NE(nullptr, receiver);
    if (receiver)
    {
        EXPECT_EQ(RTP_OK, receiver->install_receive_hook(tester, hook));
    }
}

inline void rtp_receive_hook(void* arg, uvgrtp::frame::rtp_frame* frame)
{
    if (arg != nullptr)
    {
        Test_receiver* tester = (Test_receiver*)arg;
        tester->receive();
    }

    process_rtp_frame(frame);
}

inline void process_rtp_frame(uvgrtp::frame::rtp_frame* frame)
{
    EXPECT_NE(0, frame->payload_len);
    EXPECT_EQ(2, frame->header.version);
    (void)uvgrtp::frame::dealloc_frame(frame);
}

inline void set_nal_unit(uint8_t* frame, size_t& pos, bool start_code, uint8_t zeros,
    uint8_t first_byte, uint8_t second_byte)
{
    if (start_code)
    {
        memset(frame + pos, 0, zeros);
        pos += zeros;
        memset(frame + pos, 1, 1);
        pos += 1;
    }

    memset(frame + pos, first_byte, 1);
    pos += 1;
    memset(frame + pos, second_byte, 1);
    pos += 1;
}

inline void set_nal_unit(uint8_t* frame, size_t& pos, bool start_code, uint8_t zeros,
    uint8_t first_byte)
{
    if (start_code)
    {
        memset(frame + pos, 0, zeros);
        pos += zeros;
        memset(frame + pos, 1, 1);
        pos += 1;
    }

    memset(frame + pos, first_byte, 1);
    pos += 1;
}