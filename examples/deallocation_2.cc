#include <uvgrtp/lib.hh>

#define PAYLOAD_MAXLEN 100

int main(void)
{
    /* See sending.cc for more details */
    uvg_rtp::context ctx;

    /* See sending.cc for more details */
    uvg_rtp::session *sess = ctx.create_session("127.0.0.1");

    /* Pass "RCE_SYSTEM_CALL_DISPATCHER" to flags to indicate that we want to spawn for SCD for this media stream.
     * SCD requires that we provide some deallocation mechanism
     *
     * This example is about the copy approach
     *
     * See sending.cc for more details about media stream initialization */
    uvg_rtp::media_stream *hevc = sess->create_stream(8888, 8889, RTP_FORMAT_HEVC, RCE_SYSTEM_CALL_DISPATCHER);

    uint8_t *buffer = new uint8_t[PAYLOAD_MAXLEN];

    for (int i = 0; i < 10; ++i) {
        /* Notice the "RTP_COPY" flag passed to push_frame().
         * This forces uvgRTP to make copy of the input frame before doing anything else  */
        if (hevc->push_frame(buffer, PAYLOAD_MAXLEN, RTP_COPY) != RTP_OK) {
            fprintf(stderr, "Failed to send RTP frame!");
        }
    }

    /* Session must be destroyed manually */
    ctx.destroy_session(sess);

    return 0;
}
