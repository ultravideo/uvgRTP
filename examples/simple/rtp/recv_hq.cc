#include <kvzrtp/lib.hh>

void receive_hook(void *arg, kvz_rtp::frame::rtp_frame *frame)
{
    (void)kvz_rtp::frame::dealloc_frame(frame);
}

int main(int argc, char **argv)
{
    /* Enable system call dispatcher to improve sending speed */
    kvz_rtp::context ctx;

    kvz_rtp::reader *reader = ctx.create_reader("127.0.0.1", 5566, RTP_FORMAT_GENERIC);

    /* Enable optimistic fragment receiver */
    reader->configure(RCE_OPTIMISTIC_RECEIVER);

    /* Increase the send UDP buffer size to 40 MB */
    reader->configure(RCC_UDP_BUF_SIZE, 40 * 1024 * 1024);

    /* Allocate space for 30 frames at the beginning of frame before they're
     * spilled to temporary frames */
    reader->configure(RCC_PROBATION_ZONE_SIZE, 30);

    reader->install_recv_hook(NULL, receive_hook);

    (void)reader->start();
    (void)ctx.destroy_reader(reader);

    return 0;
}
