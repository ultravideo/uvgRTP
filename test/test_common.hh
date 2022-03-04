#pragma once

#include <gtest/gtest.h>
#include "uvgrtp/lib.hh"

void wait_until_next_frame(std::chrono::steady_clock::time_point& start, 
    int frame_index, int packet_interval_ms);

inline void send_packets(uvgrtp::session* sess, uvgrtp::media_stream* sender,
    int packets, size_t size, int packet_interval_ms);
inline void add_hook(uvgrtp::media_stream* receiver, void (*hook)(void*, uvgrtp::frame::rtp_frame*));

inline void cleanup_sess(uvgrtp::context& ctx, uvgrtp::session* sess);
inline void cleanup_ms(uvgrtp::session* sess, uvgrtp::media_stream* ms);

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

    bool gotAll()
    {
        return receivedPackets_ == expectedPackets_;
    }

private:

    int receivedPackets_;
    int expectedPackets_;
};

inline void send_packets(uvgrtp::session* sess, uvgrtp::media_stream* sender, 
    int packets, size_t size, int packet_interval_ms)
{
    EXPECT_NE(nullptr, sess);
    EXPECT_NE(nullptr, sender);
    if (sess && sender)
    {
        std::cout << "Sending " << packets << " test packets with size " << size << std::endl;
        
        std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
        for (unsigned int i = 0; i < packets; ++i)
        {
            std::unique_ptr<uint8_t[]> dummy_frame = std::unique_ptr<uint8_t[]>(new uint8_t[size]);
            memset(dummy_frame.get(), 'a', size);

            if (sender->push_frame(std::move(dummy_frame), size, RTP_NO_FLAGS) != RTP_OK)
            {
                std::cout << "Failed to send test packet!" << std::endl;
            }

            if (i % (packets / 10) == packets / 10 - 1)
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

inline void wait_until_next_frame(std::chrono::steady_clock::time_point& start, int frame_index, int packet_interval_ms)
{
    // wait until it is time to send the next frame. Simulates a steady sending pace
    // and included only for demostration purposes since you can use uvgRTP to send
    // packets as fast as desired
    auto time_since_start = std::chrono::steady_clock::now() - start;
    auto next_frame_time = (frame_index + 1) * std::chrono::milliseconds(packet_interval_ms);
    if (next_frame_time > time_since_start)
    {
        std::this_thread::sleep_for(next_frame_time - time_since_start);
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


inline void add_hook(uvgrtp::media_stream* receiver, 
    void (*hook)(void*, uvgrtp::frame::rtp_frame*))
{
    EXPECT_NE(nullptr, receiver);
    if (receiver)
    {
        std::cout << "Installing hook" << std::endl;
        EXPECT_EQ(RTP_OK, receiver->install_receive_hook(nullptr, hook));
    }
}