#include <uvgrtp/lib.hh>

#include <thread>

constexpr uint16_t LOCAL_PORT = 8890;

constexpr char REMOTE_ADDRESS[] = "127.0.0.1";
constexpr uint16_t REMOTE_PORT = 8888;

// This example runs for 10 seconds
constexpr auto RECEIVE_TIME_MS = std::chrono::milliseconds(10000);


void rtp_receive_hook(void *arg, uvgrtp::frame::rtp_frame *frame)
{
    std::cout << "Received RTP frame" << std::endl;

    /* Now we own the frame. Here you could give the frame to the application
     * if f.ex "arg" was some application-specific pointer
     *
     * arg->copy_frame(frame) or whatever
     *
     * When we're done with the frame, it must be deallocated manually */
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
    uvgrtp::media_stream *hevc = sess->create_stream(LOCAL_PORT, REMOTE_PORT, RTP_FORMAT_H265, flags);

    /* Receive hook can be installed and uvgRTP will call this hook when an RTP frame is received
     *
     * This is a non-blocking operation
     *
     * If necessary, receive hook can be given an argument and this argument is supplied to
     * the receive hook every time the hook is called. This argument could a pointer to application-
     * specfic object if the application needs to be called inside the hook
     *
     * If it's not needed, it should be set to nullptr */
    hevc->install_receive_hook(nullptr, rtp_receive_hook);

    std::cout << "Waiting incoming packets for " << RECEIVE_TIME_MS.count() << " ms" << std::endl;

    std::this_thread::sleep_for(RECEIVE_TIME_MS);

    if (hevc)
    {
        sess->destroy_stream(hevc);
    }

    if (sess)
    {
        /* Session must be destroyed manually */
        ctx.destroy_session(sess);
    }

    return 0;
}
