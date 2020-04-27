#include <uvgrtp/lib.hh>

#define PAYLOAD_MAXLEN 100

int main(void)
{
    /* See sending.cc for more details */
    uvg_rtp::context ctx;

    /* See sending.cc for more details */
    uvg_rtp::session *sess = ctx.create_session("127.0.0.1");

    /* Pass "RCE_SYSTEM_CALL_DISPATCHER" to flags to indicate that we want to spawn SCD for this media stream.
     * SCD requires that we provide some deallocation mechanism, this example is about the smart pointer approach 
     *
     * See sending.cc for more details about media stream initialization */
    uvg_rtp::media_stream *hevc = sess->create_stream(8888, 8889, RTP_FORMAT_HEVC, RCE_SYSTEM_CALL_DISPATCHER);

    for (int i = 0; i < 10; ++i) {
        std::unique_ptr<uint8_t[]> buffer = std::unique_ptr<uint8_t[]>(new uint8_t[PAYLOAD_MAXLEN]);

        /* This is very similiar to sending.cc but here we must use std::move to give the unique_ptr to uvgRTP 
         * We can no longer use buffer and must reallocate new memory chunk on the next iteration. 
         *
         * The memory is deallocated automatically when system call dispatcher has finished processing the transaction */
        if (hevc->push_frame(std::move(buffer), PAYLOAD_MAXLEN, RTP_NO_FLAGS) != RTP_OK) {
            fprintf(stderr, "Failed to send RTP frame!");
        }
    }

    /* Session must be destroyed manually */
    ctx.destroy_session(sess);

    return 0;
}
