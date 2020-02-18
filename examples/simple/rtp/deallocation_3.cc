#include <kvzrtp/lib.hh>

#define PAYLOAD_MAXLEN 100

int main(void)
{
    /* See sending.cc for more details */
    kvz_rtp::context ctx;

    /* See sending.cc for more details */
    kvz_rtp::session *sess = ctx.create_session("127.0.0.1");

    /* Pass "RCE_SYSTEM_CALL_DISPATCHER" to flags to indicate that we want to spawn for SCD for this media stream.
     * SCD requires that we provide some deallocation mechanism
     *
     * This example is about the deallocation hook approach
     *
     * See sending.cc for more details about media stream initialization */
    kvz_rtp::media_stream *hevc = sess->create_stream(8888, 8889, RTP_FORMAT_HEVC, RCE_SYSTEM_CALL_DISPATCHER);

    /* When SCD has processed this memory chunk, it will call dealloc_hook()
     * which will do all the necessary deallocation steps required by the application 
     *
     * Application is not allowed to deallocate the memory chunk without kvzRTP's explicit permission */
    hevc->install_deallocation_hook(dealloc_hook);

    uint8_t *buffer = new uint8_t[PAYLOAD_MAXLEN];

    for (int i = 0; i < 10; ++i) {
        if (hevc->push_frame(buffer, PAYLOAD_MAXLEN, RTP_NO_FLAGS) != RTP_OK) {
            fprintf(stderr, "Failed to send RTP frame!");
        }
    }

    /* Session must be destroyed manually */
    ctx.destroy_session(sess);

    return 0;
}
