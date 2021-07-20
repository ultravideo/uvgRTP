#include <uvgrtp/lib.hh>

#include <iostream>
#include <thread>


constexpr uint16_t LOCAL_PORT = 8890;

constexpr char REMOTE_ADDRESS[] = "127.0.0.1";
constexpr uint16_t REMOTE_PORT = 8888;

constexpr auto RECEIVE_TIME_MS = std::chrono::milliseconds(10000);
constexpr int RECEIVER_WAIT_TIME_MS = 100;

void process_frame(uvgrtp::frame::rtp_frame *frame)
{
    std::cout << "Received an RTP frame" << std::endl;

    /* When we receive a frame, the ownership of the frame belongs to us and
     * when we're done with it, we need to deallocate the frame */
    (void)uvgrtp::frame::dealloc_frame(frame);
}

int main(void)
{
    std::cout << "Starting uvgRTP RTP receive hook example" << std::endl;

    /* See sending.cc for more details */
    uvgrtp::context ctx;

    /* See sending.cc for more details */
    uvgrtp::session *sess = ctx.create_session(REMOTE_ADDRESS);

    /* See sending.cc for more details */
    int flags = RTP_NO_FLAGS;
    uvgrtp::media_stream *hevc = sess->create_stream(LOCAL_PORT, REMOTE_PORT,
                                                     RTP_FORMAT_H265, flags);

    if (hevc)
    {
        uvgrtp::frame::rtp_frame *frame = nullptr;

        std::cout << "Start receiving frames for " << RECEIVE_TIME_MS.count() << " ms" << std::endl;
        auto start = std::chrono::steady_clock::now();

        while (std::chrono::steady_clock::now() - start < RECEIVE_TIME_MS)
        {
            /* You can specify a timeout for the operation and if the a frame is not received
             * within that time limit, pull_frame() returns a nullptr
             *
             * The parameter tells how long time a frame is waited in milliseconds */

            frame = hevc->pull_frame(RECEIVER_WAIT_TIME_MS);
            if (frame)
                process_frame(frame);
        }


         sess->destroy_stream(hevc);
    }

    if (sess)
    {
        /* Session must be destroyed manually */
        ctx.destroy_session(sess);
    }

    return EXIT_SUCCESS;
}
