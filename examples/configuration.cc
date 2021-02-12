#include <uvgrtp/lib.hh>

#define PAYLOAD_MAXLEN 4096

int main(void)
{
    /* To use the library, one must create a global RTP context object */
    uvg_rtp::context ctx;

    /* Each new IP address requires a separate RTP session.
     * This session object contains all media streams and an RTCP object (if enabled) */
    uvg_rtp::session *sess = ctx.create_session("127.0.0.1");

    /* Some of the functionality of uvgRTP can be enabled/disabled using RCE_* flags.
     *
     * For example, here the created MediaStream object has RTCP enabled,
     * does not utilize system call clustering to reduce the possibility of packet dropping
     * and prepends a 4-byte HEVC start code (0x00000001) before each NAL unit */
    unsigned flags =
        RCE_RTCP |                      /* enable RTCP */
        RCE_NO_SYSTEM_CALL_CLUSTERING | /* disable System Call Clustering */
        RCE_H26X_PREPEND_SC;            /* prepend start code to each returned HEVC frame */

    uvg_rtp::media_stream *hevc = sess->create_stream(8888, 8889, RTP_FORMAT_H265, flags);

    /* uvgRTP context can also be configured using RCC_* flags
     * These flags do not enable/disable functionality but alter default behaviour of uvgRTP
     *
     * For example, here UDP send/recv buffers are increased to 40MB
     * and frame delay is set 150 milliseconds to allow frames to arrive a little late */
    hevc->configure_ctx(RCC_UDP_RCV_BUF_SIZE, 40 * 1000 * 1000);
    hevc->configure_ctx(RCC_UDP_SND_BUF_SIZE, 40 * 1000 * 1000);
    hevc->configure_ctx(RCC_PKT_MAX_DELAY,                 150);

    for (;;) {
        auto buffer = std::unique_ptr<uint8_t[]>(new uint8_t[PAYLOAD_MAXLEN]);

        if (hevc->push_frame(std::move(buffer), PAYLOAD_MAXLEN, RTP_NO_FLAGS) != RTP_OK)
            fprintf(stderr, "Failed to send RTP frame!");
    }

    /* Session must be destroyed manually */
    ctx.destroy_session(sess);

    return 0;
}
