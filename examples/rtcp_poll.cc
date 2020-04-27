#include <uvgrtp/lib.hh>
#include <cstring>

/* The poller is run in a separate thread to prevent blocking of main thread
 * Polling is not the advised way of handling RTCP packet retrival
 *
 * See rtcp/rtcp_hook.cc */
void rtcp_thread(uvg_rtp::writer *writer)
{
    fprintf(stderr, "Staring RTCP Receiver Report poller...\n");

    uvg_rtp::rtcp *rtcp = writer->get_rtcp();

    while (true) {
        /* Participants must be polled on
         * every iterations as people may join/leave at any point */
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

        std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    }
}

int main(void)
{
    /* See rtp/sending.cc for information about session initialization */
    uvg_rtp::context rtp_ctx;

    uvg_rtp::writer *writer = rtp_ctx.create_writer("127.0.0.1", 8888, RTP_FORMAT_GENERIC);
    uvg_rtp::reader *reader = rtp_ctx.create_reader("127.0.0.1", 8888, RTP_FORMAT_GENERIC);

    /* Send Sender Reports 127.0.0.1:8889 and receive RTCP Reports to 5566 */
    (void)writer->create_rtcp("127.0.0.1", 8889, 5566);

    /* Send Receiver Reports 127.0.0.1:5566 and receive RTCP Reports to 8889 */
    (void)reader->create_rtcp("127.0.0.1", 5566, 8889);

    /* Create separate thread for polling the RTCP packets. In this example,
     * we only create thread for RTCP Receiver Reports (so for RTP Sender).
     *
     * Polling must be obviously done in a separate thread because it's a blocking operation */
    std::thread *t1 = new std::thread(rtcp_thread, writer);

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
