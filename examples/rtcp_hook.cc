#include <kvzrtp/lib.hh>
#include <cstring>

/* kvzRTP calls this hook when it receives an RTCP Sender Report
 * In this example, this doesn't get called because there's only one sender
 *
 * NOTE: If application uses hook, it must also free the frame when it's done with i
 * Frame must deallocated using kvz_rtp::frame::dealloc_frame() function */
void sender_hook(kvz_rtp::frame::rtcp_sender_frame *frame)
{
    fprintf(stderr, "Got RTCP Sender Report\n");

    (void)kvz_rtp::frame::dealloc_frame(frame);
}
/* kvzRTP calls this hook when it receives an RTCP Receiver Report
 *
 * NOTE: If application uses hook, it must also free the frame when it's done with i
 * Frame must deallocated using kvz_rtp::frame::dealloc_frame() function */
void receiver_hook(kvz_rtp::frame::rtcp_receiver_frame *frame)
{
    fprintf(stderr, "got RTCP Receiver Report\n");

    (void)kvz_rtp::frame::dealloc_frame(frame);
}

int main(void)
{
    /* See rtp/sending.cc for more information about session initialization */
    kvz_rtp::context rtp_ctx;

    kvz_rtp::writer *writer = rtp_ctx.create_writer("127.0.0.1", 8888, RTP_FORMAT_GENERIC);
    kvz_rtp::reader *reader = rtp_ctx.create_reader("127.0.0.1", 8888, RTP_FORMAT_GENERIC);

    /* Send Sender Reports 127.0.0.1:8889 and receive RTCP Reports to 5566
     * Add hook for the Receiver Report */
    (void)writer->create_rtcp("127.0.0.1", 8889, 5566);
    (void)writer->get_rtcp()->install_receiver_hook(receiver_hook);

    /* Send Receiver Reports 127.0.0.1:5566 and receive RTCP Reports to 8889
     * and add hook for the Sender Report */
    (void)reader->create_rtcp("127.0.0.1", 5566, 8889);
    (void)reader->get_rtcp()->install_sender_hook(sender_hook);

    (void)writer->start();
    (void)reader->start();

    /* Send dummy data so there's some RTCP data to send */
    uint8_t buffer[50] = { 0 };
    memset(buffer, 'a', 50);

    while (true) {
        writer->push_frame((uint8_t *)buffer, 50, RTP_NO_FLAGS);
        std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    }
}
