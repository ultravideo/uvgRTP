#include <uvgrtp/lib.hh>
#include <thread>

int main(void)
{
    /* See sending.cc for more details */
    uvgrtp::context ctx;

    /* See sending.cc for more details */
    uvgrtp::session *sess = ctx.create_session("127.0.0.1");

    /* See sending.cc for more details */
    uvgrtp::media_stream *hevc = sess->create_stream(8888, 8889, RTP_FORMAT_H265, 0);

    /* pull_frame() will block until a frame is received.
     *
     * If that is not acceptable, a separate thread for the reader should be created */
    uvgrtp::frame::rtp_frame *frame = nullptr;

    while (!(frame = hevc->pull_frame())) {
        /* When we receive a frame, the ownership of the frame belongs to us and
         * when we're done with it, we need to deallocate the frame */
        (void)uvgrtp::frame::dealloc_frame(frame);
    }

    /* You can also specify for a timeout for the operation and if the a frame is not received 
     * within that time limit, pull_frame() returns a nullptr 
     *
     * The parameter tells how long time a frame is waited in milliseconds */
    frame = hevc->pull_frame(200);

    /* Frame must be freed manually */
    uvgrtp::frame::dealloc_frame(frame);

    ctx.destroy_session(sess);
    return 0;
}
