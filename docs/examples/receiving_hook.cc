#include <uvgrtp/lib.hh>
#include <thread>

void receive_hook(void *arg, uvgrtp::frame::rtp_frame *frame)
{
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
    /* See sending.cc for more details */
    uvgrtp::context ctx;

    /* See sending.cc for more details */
    uvgrtp::session *sess = ctx.create_session("127.0.0.1");

    /* See sending.cc for more details */
    uvgrtp::media_stream *hevc = sess->create_stream(8888, 8889, RTP_FORMAT_H265, 0);

    /* Receive hook can be installed and uvgRTP will call this hook when an RTP frame is received
     *
     * This is a non-blocking operation
     *
     * If necessary, receive hook can be given an argument and this argument is supplied to
     * the receive hook every time the hook is called. This argument could a pointer to application-
     * specfic object if the application needs to be called inside the hook
     *
     * If it's not needed, it should be set to nullptr */
    hevc->install_receive_hook(nullptr, receive_hook);

    /* Session must be destroyed manually */
    ctx.destroy_session(sess);

    return 0;
}
