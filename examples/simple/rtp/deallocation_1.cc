#include <kvzrtp/lib.hh>

#define PAYLOAD_MAXLEN 100

int main(int argc, char **argv)
{
    kvz_rtp::context ctx;

    kvz_rtp::writer *writer = ctx.create_writer("127.0.0.1", 5566, RTP_FORMAT_GENERIC);

    (void)writer->start();

    for (int i = 0; i < 10; ++i) {
        std::unique_ptr<uint8_t[]> buffer = std::unique_ptr<uint8_t[]>(new uint8_t[PAYLOAD_MAXLEN]);

        /* This is very similiar to sending.cc but here we must use std::move to give the unique_ptr to kvzRTP 
         * We can no longer use buffer and must reallocate new memory chunk on next iteration. 
         *
         * The memory is deallocated automatically when system call dispatcher has finished processing the transaction */
        if (writer->push_frame(std::move(buffer), PAYLOAD_MAXLEN, RTP_NO_FLAGS) != RTP_OK) {
            fprintf(stderr, "Failed to send RTP frame!");
        }
    }

    /* Writer must be destroyed manually */
    ctx.destroy_writer(writer);

    return 0;
}
