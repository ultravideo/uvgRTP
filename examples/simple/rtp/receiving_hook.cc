#include <kvzrtp/lib.hh>
#include <thread>

#define USE_RECV_HOOK

void receive_hook(void *arg, kvz_rtp::frame::rtp_frame *frame)
{
    if (!frame) {
        fprintf(stderr, "invalid frame received!\n");
        return;
    }

    /* Now we own the frame. Here you could give the frame the application 
     * if f.ex arg pointed to some application-specfic pointer
     *
     * arg->copy_frame(frame) etc.
     *
     * When we're done with the frame, it must be deallocated */
    (void)kvz_rtp::frame::dealloc_frame(frame);
}

int main(int argc, char **argv)
{
    (void)argc, (void)argv;

    /* See sending.cc for information about the session initialization */
    kvz_rtp::context ctx;

    /* Initialization for both receiving styles is similar */
    kvz_rtp::reader *reader = ctx.create_reader("127.0.0.1", 5566, RTP_FORMAT_GENERIC);

    /* Receive hook can be installed and the receiver will call this hook 
     * and an RTP frame is received 
     *
     * This is a non-blocking operation
     *
     * If necessary, receive hook can be given an argument and this argument is supplied to
     * receive hook every time the hook is called. This argument could a pointer to application-
     * specfic object if the application needs to be called inside the hook
     *
     * If it's not needed, it should be set to NULL */
    reader->install_recv_hook(NULL, receive_hook);

    /* Now that the receive hook is in place, reader can be started */
    (void)reader->start();

    /* NOTE: Because we've only initalized reader, this example code will return immediately 
     * because there's nothing blocking the main thread.
     *
     * Create RTP Sender (seen rtp/sending.cc) if you wish to construct full working 
     * example code with both sender and receiver */

    /* Reader object must be destroy explicitly */
    (void)ctx.destroy_reader(reader);
    
    return 0;
}
