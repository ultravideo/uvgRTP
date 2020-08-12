#include <uvgrtp/lib.hh>
#include <cstring>

/* The poller is run in a separate thread to prevent blocking of main thread
 * Polling is not the advised way of handling RTCP packet retrival
 *
 * See rtcp/rtcp_hook.cc */
void rtcp_thread(uvg_rtp::rtcp *rtcp)
{
    for (;;) {
        auto ssrcs = rtcp->get_participants();

        for (auto& i : ssrcs) {
            auto report_block = rtcp->get_receiver_packet(i);

            /* If a Receiver Report is received, it must be deallocated manually by
             * calling uvg_rtp::frame::dealloc_frame() */
            if (report_block) {
                fprintf(stderr, "got receiver packet\n");
                (void)uvg_rtp::frame::dealloc_frame(report_block);
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }
}

int main(void)
{
    /* See rtp/sending.cc for information about session initialization */
    uvg_rtp::context ctx;

    uvg_rtp::session *sess = ctx.create_session("127.0.0.1");

    uvg_rtp::media_stream *s1 = sess->create_stream(7777, 8888, RTP_FORMAT_GENERIC, RTP_NO_FLAGS);
    uvg_rtp::media_stream *s2 = sess->create_stream(8888, 7777, RTP_FORMAT_GENERIC, RTP_NO_FLAGS);

    /* Create separate thread for polling the RTCP packets. In this example,
     * we only create thread for RTCP Receiver Reports (so for RTP Sender).
     *
     * Polling must be obviously done in a separate thread because it's a blocking operation */
    std::thread *t1 = new std::thread(rtcp_thread, s1->get_rtcp());

    /* Send dummy data so there's some RTCP data to send */
    uint8_t buffer[50] = { 0 };
    memset(buffer, 'a', 50);

    while (true) {
        s1->push_frame((uint8_t *)buffer, 50, RTP_NO_FLAGS);
        std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    }
}
