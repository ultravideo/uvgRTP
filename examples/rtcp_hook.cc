#include <uvgrtp/lib.hh>
#include <cstring>

/* uvgRTP calls this hook when it receives an RTCP Sender Report
 * In this example, this doesn't get called because there's only one sender
 *
 * NOTE: If application uses hook, it must also free the frame when it's done with i
 * Frame must deallocated using uvg_rtp::frame::dealloc_frame() function */
void sender_hook(uvg_rtp::frame::rtcp_sender_frame *frame)
{
    fprintf(stderr, "Got RTCP Sender Report\n");

    (void)uvg_rtp::frame::dealloc_frame(frame);
}
/* uvgRTP calls this hook when it receives an RTCP Receiver Report
 *
 * NOTE: If application uses hook, it must also free the frame when it's done with i
 * Frame must deallocated using uvg_rtp::frame::dealloc_frame() function */
void receiver_hook(uvg_rtp::frame::rtcp_receiver_frame *frame)
{
    fprintf(stderr, "got RTCP Receiver Report\n");

    (void)uvg_rtp::frame::dealloc_frame(frame);
}

int main(void)
{
    /* See rtp/sending.cc for more information about session initialization */
    uvg_rtp::context ctx;

    uvg_rtp::session *sess = ctx.create_session("127.0.0.1");

    uvg_rtp::media_stream *s1 = sess->create_stream(7777, 8888, RTP_FORMAT_GENERIC, RTP_NO_FLAGS);
    uvg_rtp::media_stream *s2 = sess->create_stream(8888, 7777, RTP_FORMAT_GENERIC, RTP_NO_FLAGS);

    s1->create_rtcp(7778, 8889);
    s2->create_rtcp(8889, 7778);

    /* Send Sender Reports 127.0.0.1:8889 and receive RTCP Reports to 5566
     * Add hook for the Receiver Report */
    /* (void)writer->create_rtcp("127.0.0.1", 8889, 5566); */
    (void)s1->get_rtcp()->install_receiver_hook(receiver_hook);
    (void)s1->get_rtcp()->install_sender_hook(sender_hook);

    /* Send Receiver Reports 127.0.0.1:5566 and receive RTCP Reports to 8889
     * and add hook for the Sender Report */
    /* (void)reader->create_rtcp("127.0.0.1", 5566, 8889); */
    (void)s2->get_rtcp()->install_sender_hook(sender_hook);
    (void)s2->get_rtcp()->install_receiver_hook(receiver_hook);

    /* Send dummy data so there's some RTCP data to send */
    uint8_t buffer[50] = { 0 };
    memset(buffer, 'a', 50);

    while (true) {
        s1->push_frame((uint8_t *)buffer, 50, RTP_NO_FLAGS);
        std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    }
}
