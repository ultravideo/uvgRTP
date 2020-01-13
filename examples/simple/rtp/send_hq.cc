#include <kvzrtp/lib.hh>

#define PAYLOAD_MAXLEN 1 * 1000 * 1000

int main(int argc, char **argv)
{
    kvz_rtp::context ctx;

    kvz_rtp::writer *writer = ctx.create_writer("127.0.0.1", 5566, 8888, RTP_FORMAT_HEVC);

    /* Enable system call dispatcher to improve sending speed */
    writer->configure(RCE_SYSTEM_CALL_DISPATCHER);

    /* Increase the send UDP buffer size to 40 MB */
    writer->configure(RCC_UDP_BUF_SIZE, 40 * 1024 * 1024);

    /* Cache 30 transactions to prevent constant (de)allocation */
    writer->configure(RCC_MAX_TRANSACTIONS, 30);

    /* Set max size for one input frame to 1.4 MB (1441 * 1000)
     * (1.4 MB frame would equal 43 MB bitrate for a 30 FPS video which is very large) */
    writer->configure(RCC_MAX_MESSAGES, 1000);

    /* Before the writer can be used, it must be started. 
     * This initializes the underlying socket and all needed data structures */
    (void)writer->start();

    for (int i = 0; i < 10; ++i) {

        /* We're using System Call Dispatcher so we must adhere to the memory ownership/deallocation
         * rules defined in README.md
         *
         * Easiest way is to use smart pointers (as done here). If this memory was, however, received
         * from f.ex. HEVC encoder directly and was not wrapped in a smart pointer, we could either
         * install a deallocation hook for the memory or pass RTP_COPY to push_frame() to force kvzRTP
         * to make a copy of the memory */
        auto buffer = std::unique_ptr<uint8_t[]>(new uint8_t[PAYLOAD_MAXLEN]);

        /* We're using  */
        if (writer->push_frame(std::move(buffer), PAYLOAD_MAXLEN, RTP_NO_FLAGS) != RTP_OK) {
            fprintf(stderr, "Failed to send RTP frame!");
        }
    }

    /* Writer must be destroyed manually */
    ctx.destroy_writer(writer);

    return 0;
}
