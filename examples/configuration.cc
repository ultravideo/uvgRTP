#include <uvgrtp/lib.hh>

#define PAYLOAD_MAXLEN 4096

int main(void)
{
    /* To use the library, one must create a global RTP context object */
    uvg_rtp::context ctx;

    /* Each new IP address requires a separate RTP session.
     * This session objects contains all media streams and an RTCP object (if enabled) */
    uvg_rtp::session *sess = ctx.create_session("127.0.0.1");

    /* Enable system call dispatcher for the sender to minimize delay experienced by the application
     *
     * See sending.cc for more details about media stream initialization */
    uvg_rtp::media_stream *hevc = sess->create_stream(8888, 8889, RTP_FORMAT_H265, RCE_SYSTEM_CALL_DISPATCHER);

    /* Increase UDP send/recv buffers to 40 MB */
    hevc->configure_ctx(RCC_UDP_RCV_BUF_SIZE, 40 * 1000 * 1000);
    hevc->configure_ctx(RCC_UDP_SND_BUF_SIZE, 40 * 1000 * 1000);

    for (int i = 0; i < 1024; ++i) {
        /* Because we're using SCD, one of the three deallocation methods must be used,
         * here we use unique_ptrs */
        auto buffer = std::unique_ptr<uint8_t[]>(new uint8_t[PAYLOAD_MAXLEN]);

        if (hevc->push_frame(std::move(buffer), PAYLOAD_MAXLEN, RTP_NO_FLAGS) != RTP_OK)
            fprintf(stderr, "Failed to send RTP frame!");
    }

    /* Session must be destroyed manually */
    ctx.destroy_session(sess);

    return 0;
}
