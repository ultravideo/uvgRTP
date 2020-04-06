#include <kvzrtp/lib.hh>
#include <thread>

void receive_hook(void *arg, kvz_rtp::frame::rtp_frame *frame)
{
    if (!frame) {
        fprintf(stderr, "invalid frame received!\n");
        return;
    }

    /* Now we own the frame. Here you could give the frame to the application
     * if f.ex "arg" was some application-specfic pointer
     *
     * arg->copy_frame(frame) etc.
     *
     * When we're done with the frame, it must be deallocated */
    (void)kvz_rtp::frame::dealloc_frame(frame);
}

int main(void)
{
    /* To use the library, one must create a global RTP context object */
    kvz_rtp::context ctx;

    /* Each new IP address requires a separate RTP session.
     * This session objects contains all media streams and an RTCP object (if enabled) */
    kvz_rtp::session *sess = ctx.create_session("127.0.0.1");

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
     * In this example, we have one media streams with remote participant: hevc */
    kvz_rtp::media_stream *hevc = sess->create_stream(8888, 8889, RTP_FORMAT_HEVC, 0);

    /* Receive hook can be installed and the receiver will call this hook when an RTP frame is received
     *
     * This is a non-blocking operation
     *
     * If necessary, receive hook can be given an argument and this argument is supplied to
     * receive hook every time the hook is called. This argument could a pointer to application-
     * specfic object if the application needs to be called inside the hook
     *
     * If it's not needed, it should be set to nullptr */
    hevc->install_receive_hook(nullptr, receive_hook);

    /* Session must be destroyed manually */
    ctx.destroy_session(sess);

    return 0;
}
