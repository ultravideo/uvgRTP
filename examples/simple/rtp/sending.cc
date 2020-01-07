#include <kvzrtp/lib.hh>

#define PAYLOAD_MAXLEN 100

int main(int argc, char **argv)
{
    /* To use the library, one must create one global RTP context object
     * This object is used to create RTP readers/writers */
    kvz_rtp::context ctx;

    /* Creating a writer is very simple: all that needs to be provided is the
     * destination address and port, and media type
     *
     * "RTP_FORMAT_GENERIC" means that no assumptions should be made about the data
     * and it should be sent as it.
     * This can be used for example raw video/raw audio or for media formats not supported
     * by the library
     *
     * If "RTP_FORMAT_HEVC" is provided, push_frame() splits the buffer into NAL units
     * and sends these NAL units in one or more RTP frames (if NAL unit size > MTU,
     * the NAL unit is split into fragments)
     * HEVC frame receiver will reconstruct the frame from these fragments and return
     * complete NAL units back
     *
     * Source port is an optional argument that can be provided if UDP hole
     * punching is utilized */
    kvz_rtp::writer *writer = ctx.create_writer("127.0.0.1", 5566, 8888, RTP_FORMAT_GENERIC);

    /* Before the writer can be used, it must be started. 
     * This initializes the underlying socket and all needed data structures */
    (void)writer->start();

    uint8_t *buffer = new uint8_t[PAYLOAD_MAXLEN];

    for (int i = 0; i < 10; ++i) {

        /* Sending data is as simple as calling push_frame() */
        if (writer->push_frame(buffer, PAYLOAD_MAXLEN, RTP_NO_FLAGS) != RTP_OK) {
            fprintf(stderr, "Failed to send RTP frame!");
        }
    }

    /* Writer must be destroyed manually */
    delete[] buffer;
    ctx.destroy_writer(writer);

    return 0;
}
