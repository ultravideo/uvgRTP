#include <uvgrtp/lib.hh>
#include <cstring>

/* uvgRTP calls this hook when it receives an RTCP Receiver Report
 *
 * NOTE: If application uses hook, it must also free the frame when it's done with i
 * Frame must deallocated using uvgrtp::frame::dealloc_frame() function */
void receiver_hook(uvgrtp::frame::rtcp_receiver_report *frame)
{
    LOG_INFO("Received an RTCP Receiver Report");

    /* RTCP frames can be deallocated using delete */
    delete frame;
}

int main(void)
{
    /* See rtp/sending.cc for more information about session initialization */
    uvgrtp::context ctx;

    uvgrtp::session *sess = ctx.create_session("127.0.0.1");

    /* For s1, RTCP runner is using port 7778 and for s2 port 8889 */
    uvgrtp::media_stream *s1 = sess->create_stream(7777, 8888, RTP_FORMAT_GENERIC, RCE_RTCP);
    uvgrtp::media_stream *s2 = sess->create_stream(8888, 7777, RTP_FORMAT_GENERIC, RCE_RTCP);

    /* In this example code, s1 acts as the sender and because it is the only sender,
     * it does not send any RTCP frames but only receives RTCP Receiver reports from s2.
     *
     * Because s1 only sends and s2 only receives, we only need to install receive hook for s1
     *
     * By default, all media_stream that have RTCP enabled start as receivers and only if/when they 
     * call push_frame() are they converted into senders. */
    (void)s1->get_rtcp()->install_receiver_hook(receiver_hook);

    /* Send dummy data so there's some RTCP data to send */
    uint8_t buffer[50] = { 0 };
    memset(buffer, 'a', 50);

    while (true) {
        s1->push_frame((uint8_t *)buffer, 50, RTP_NO_FLAGS);
        std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    }
}
