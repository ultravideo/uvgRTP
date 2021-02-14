#include <uvgrtp/lib.hh>
#include <thread>

int main(void)
{
    /* To use the library, one must create a global RTP context object */
    uvg_rtp::context ctx;

    /* Each new IP address requires a separate RTP session.
     * This session object contains all media streams and an RTCP object (if enabled) */
    uvg_rtp::session *sess = ctx.create_session("127.0.0.1");

    /* Each RTP session has one or more media streams. These media streams are bidirectional
     * and they require both source and destination ports for the connection. One must also
     * specify the media format for the stream and any configuration flags if needed
     *
     * If ZRTP is enabled, the first media stream instance shall do a Diffie-Hellman key exchange
     * with remote and rest of the media streams use Multistream mode. ZRTP requires that both
     * source and destination ports are known so it can perform the key exchange
     *
     * First port is source port aka the port that we listen to and second port is the port
     * that remote listens to
     *
     * This same object is used for both sending and receiving media
     *
     * In this example, we have one media stream with remote participant: HEVC */
    uvg_rtp::media_stream *hevc = sess->create_stream(8888, 8889, RTP_FORMAT_H265, 0);

    /* pull_frame() will block until a frame is received.
     *
     * If that is not acceptable, a separate thread for the reader should be created */
    uvg_rtp::frame::rtp_frame *frame = nullptr;

    while ((frame = hevc->pull_frame()) != nullptr) {
        /* When we receive a frame, the ownership of the frame belongs to us and
         * when we're done with it, we need to deallocate the frame */
        (void)uvg_rtp::frame::dealloc_frame(frame);
    }

    ctx.destroy_session(sess);

    return 0;
}
