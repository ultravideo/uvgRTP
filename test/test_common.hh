#pragma once

#include <gtest/gtest.h>
#include "uvgrtp/lib.hh"

class Test_receiver;

void wait_until_next_frame(std::chrono::steady_clock::time_point& start, 
    int frame_index, int packet_interval_ms);

inline void test_packet_size(rtp_format_t format, int packets, size_t size, uvgrtp::media_stream* sender, uvgrtp::media_stream* receiver,
    int rtp_flags);
inline void send_packets(rtp_format_t format, uvgrtp::session* sess, uvgrtp::media_stream* sender,
    int packets, size_t size, int packet_interval_ms, bool add_start_code, bool print_progress, int rtp_flags);

inline void add_hook(Test_receiver* tester, uvgrtp::media_stream* receiver, void (*hook)(void*, uvgrtp::frame::rtp_frame*));

inline void cleanup_sess(uvgrtp::context& ctx, uvgrtp::session* sess);
inline void cleanup_ms(uvgrtp::session* sess, uvgrtp::media_stream* ms);

inline void process_rtp_frame(uvgrtp::frame::rtp_frame* frame);
inline void rtp_receive_hook(void* arg, uvgrtp::frame::rtp_frame* frame);

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

inline void send_packets(rtp_format_t format, uvgrtp::session* sess, uvgrtp::media_stream* sender,
    int packets, size_t size, int packet_interval_ms, bool add_start_code, bool print_progress, int rtp_flags)
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

            memset(dummy_frame.get(), 'b', size);

            if (add_start_code && size > 8)
            {
                int pos = 0;
                if (format == RTP_FORMAT_H264)
                {
                    // https://datatracker.ietf.org/doc/html/rfc6184#section-1.3
                    if (!(rtp_flags & RTP_NO_H26X_SCL))
                    {
                        memset(dummy_frame.get() + pos, 0, 2);
                        pos += 2;
                        memset(dummy_frame.get() + pos, 1, 1);
                        pos += 1;
                    }
                    memset(dummy_frame.get() + pos, 5, 1); // Intra frame
                    pos += 1;
                }
                else if (format == RTP_FORMAT_H265)
                {
                    // see https://datatracker.ietf.org/doc/html/rfc7798#section-1.1.4
                    if (!(rtp_flags & RTP_NO_H26X_SCL))
                    {
                        memset(dummy_frame.get() + pos, 0, 3);
                        pos += 3;
                        memset(dummy_frame.get() + pos, 1, 1);
                        pos += 1;
                    }
                    memset(dummy_frame.get() + pos, (19 << 1), 1); // Intra frame
                    pos += 1;
                }
                else if (format == RTP_FORMAT_H266)
                {
                    // see https://datatracker.ietf.org/doc/html/draft-ietf-avtcore-rtp-vvc#section-1.1.4
                    if (!(rtp_flags & RTP_NO_H26X_SCL))
                    {
                        memset(dummy_frame.get() + pos, 0, 3);
                        pos += 3;
                        memset(dummy_frame.get() + pos, 1, 1);
                        pos += 1;
                    }

                    // |0|1|2|3|4|5|6|7|0|1|2|3|4|5|6|7|
                    // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
                    // |F|Z|  LayerID  |  Type   | TID |
                    memset(dummy_frame.get() + pos, 0, 1);
                    pos += 1;
                    memset(dummy_frame.get() + pos, (7 << 3), 1); // Intra frame (type)
                }
            }

            rtp_error_t ret = RTP_OK;
            if ((ret = sender->push_frame(std::move(dummy_frame), size, rtp_flags)) != RTP_OK)
            {
                std::cout << "Failed to send test packet! Return value: " << ret << std::endl;
                return;
            }

            if (i % (packets / 10) == packets / 10 - 1 && print_progress)
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


inline void test_packet_size(rtp_format_t format, int packets, size_t size, uvgrtp::session* sess, uvgrtp::media_stream* sender,
    uvgrtp::media_stream* receiver, int rtp_flags)
{
    EXPECT_NE(nullptr, sess);
    EXPECT_NE(nullptr, sender);
    EXPECT_NE(nullptr, receiver);

    if (sess && sender && receiver)
    {
        Test_receiver* tester = new Test_receiver(packets);

        int interval_ms = 10;

        add_hook(tester, receiver, rtp_receive_hook);
        send_packets(format, sess, sender, packets, size, interval_ms, true, false, rtp_flags);

        std::this_thread::sleep_for(std::chrono::milliseconds(50 + size/1000));

        tester->gotAll();
        delete tester;
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