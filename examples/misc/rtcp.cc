#include <cstring>
#include "../../src/lib.hh"

int main(void)
{
    kvz_rtp::context rtp_ctx;

    kvz_rtp::writer *writer = rtp_ctx.create_writer("127.0.0.1", 8888, RTP_FORMAT_HEVC);
    kvz_rtp::reader *reader = rtp_ctx.create_reader("127.0.0.1", 8888, RTP_FORMAT_HEVC);

    /* Send Sender Reports 127.0.0.1:8889 and receive RTCP Reports to 5566 */
    writer->create_rtcp("127.0.0.1", 8889, 5566);

    /* Send Receiver Reports 127.0.0.1:5566 and receive RTCP Reports to 8889 */
    reader->create_rtcp("127.0.0.1", 5566, 8889);

    (void)writer->start();
    (void)reader->start();

    uint8_t buffer[50] = { 0 };
    memset(buffer, 'a', 50);

    while (true) {
        writer->push_frame((uint8_t *)buffer, 50, RTP_FORMAT_GENERIC);
        std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    }
}
