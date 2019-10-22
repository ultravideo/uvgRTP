#include <kvzrtp/lib.hh>

#define PAYLOAD_MAXLEN 100

int main(int argc, char **argv)
{
    kvz_rtp::writer *writer = ctx.create_writer("127.0.0.1", 5566, 8888);

    (void)writer->start();

    uint8_t *buffer = new uint8_t[PAYLOAD_MAXLEN];

    for (int i = 0; i < 10; ++i) {
        if (writer->push_frame(buffer, PAYLOAD_MAXLEN, RTP_COPY) != RTP_OK) {
            fprintf(stderr, "Failed to send RTP frame!");
        }
    }

    /* Writer must be destroyed manually */
    delete[] buffer;
    ctx.destroy_writer(writer);

    return 0;
}
