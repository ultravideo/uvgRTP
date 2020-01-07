#include <kvzrtp/lib.hh>

#define PAYLOAD_MAXLEN 100

void dealloc_hook(void *mem)
{
    delete[] mem;
}

int main(int argc, char **argv)
{
    kvz_rtp::context ctx;

    kvz_rtp::writer *writer = ctx.create_writer("127.0.0.1", 5566, 8888, RTP_FORMAT_GENERIC);

    /* When SCD has processed this memory chunk, it will call dealloc_hook()
     * which will do all necessary deallocation steps required by the application 
     *
     * Application is not allowed to deallocate the memory chunk without kvzRTP's explicit permission */
    writer->install_dealloc_hook(dealloc_hook);
    (void)writer->start();

    for (int i = 0; i < 10; ++i) {
        uint8_t *buffer = new uint8_t[PAYLOAD_MAXLEN];

        if (writer->push_frame(buffer, PAYLOAD_MAXLEN, RTP_NO_FLAGS) != RTP_OK) {
            fprintf(stderr, "Failed to send RTP frame!");
        }
    }

    /* Writer must be destroyed manually */
    ctx.destroy_writer(writer);

    return 0;
}
