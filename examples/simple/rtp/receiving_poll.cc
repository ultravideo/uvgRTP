#include <kvzrtp/lib.hh>
#include <thread>

int main(int argc, char **argv)
{
    (void)argc, (void)argv;

    /* See sending.cc for information about the session initialization */
    kvz_rtp::context ctx;

    /* Initialization for both receiving styles is similar */
    kvz_rtp::reader *reader = ctx.create_reader("127.0.0.1", 5566, RTP_FORMAT_GENERIC);

    /* Now that the receive hook is in place, reader can be started */
    (void)reader->start();

    /* pull_frame() will block until a frame is received.
     *
     * If that is not acceptable, a separate thread for the reader should be created */
    kvz_rtp::frame::rtp_frame *frame = nullptr;

    while ((frame = reader->pull_frame()) != nullptr) {
        /* When we receive a frame, the ownership of the frame belongs to use and
         * when we're done with it, we need to deallocate the frame */
        (void)kvz_rtp::frame::dealloc_frame(frame);
    }

    /* Reader object must be destroy explicitly */
    (void)ctx.destroy_reader(reader);
    
    return 0;
}
