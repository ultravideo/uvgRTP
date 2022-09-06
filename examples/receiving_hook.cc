#include <uvgrtp/lib.hh>

#include <thread>
#include <iostream>

/* There are two main ways of getting received RTP frames from uvgRTP.
 * This example demonstrates the usage of hook function to receive RTP frames.
 *
 * The advantage of using a hook function is minimal CPU usage and delay between
 * uvgRTP receiving the frame and application processing the frame. When using
 * the hook method, the application must take care that it is not using the hook
 * function for heavy processing since this may block RTP frame reception.
 *
 * Hook based frame reception is generally recommended for most serious applications,
 * but there can be situations where polling method is better, especially if performance
 * is not a huge concern or if there needs to be tight control when the frame is
 * received by the application.
 *
 * This example only implements the receiving, but it can be used together with the
 * sending example to test the functionality.
 */

// parameters for this test. You can change these to suit your network environment
constexpr uint16_t LOCAL_PORT = 8890;

constexpr char LOCAL_ADDRESS[] = "127.0.0.1";

// This example runs for 5 seconds
constexpr auto RECEIVE_TIME_S = std::chrono::seconds(10);

void rtp_receive_hook(void *arg, uvgrtp::frame::rtp_frame *frame);
void cleanup(uvgrtp::context& ctx, uvgrtp::session *sess, uvgrtp::media_stream *receiver);

int main(void)
{
    std::cout << "Starting uvgRTP RTP receive hook example" << std::endl;

    uvgrtp::context ctx;
    uvgrtp::session *sess = ctx.create_session(LOCAL_ADDRESS);
    int flags = RCE_RECEIVE_ONLY;
    uvgrtp::media_stream *receiver = sess->create_stream(LOCAL_PORT, RTP_FORMAT_H265, flags);

    /* Receive hook can be installed and uvgRTP will call this hook when an RTP frame is received
     *
     * This is a non-blocking operation
     *
     * If necessary, receive hook can be given an argument and this argument is supplied to
     * the receive hook every time the hook is called. This argument could a pointer to application-
     * specfic object if the application needs to be called inside the hook
     *
     * If it's not needed, it should be set to nullptr */
    if (!receiver || receiver->install_receive_hook(nullptr, rtp_receive_hook) != RTP_OK)
    {
        std::cerr << "Failed to install RTP reception hook";
        cleanup(ctx, sess, receiver);
        return EXIT_FAILURE;
    }

    std::cout << "Waiting incoming packets for " << RECEIVE_TIME_S.count() << " s" << std::endl;

    std::this_thread::sleep_for(RECEIVE_TIME_S); // lets this example run for some time

    cleanup(ctx, sess, receiver);

    return EXIT_SUCCESS;
}

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

void cleanup(uvgrtp::context& ctx, uvgrtp::session *sess, uvgrtp::media_stream *receiver)
{
    if (receiver)
    {
        sess->destroy_stream(receiver);
    }

    if (sess)
    {
        /* Session must be destroyed manually */
        ctx.destroy_session(sess);
    }
}
