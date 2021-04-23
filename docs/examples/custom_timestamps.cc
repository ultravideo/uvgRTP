#include <uvgrtp/lib.hh>

#define PAYLOAD_MAXLEN 100

int main(void)
{
    /* To use the library, one must create a global RTP context object */
    uvgrtp::context ctx;

    /* See sending.cc for more details */
    uvgrtp::session *sess = ctx.create_session("127.0.0.1");

    /* Create MediaStream and enable RTCP for the stream */
    uvgrtp::media_stream *hevc = sess->create_stream(8888, 8889, RTP_FORMAT_H265, RCE_RTCP);

    uint8_t *buffer     = new uint8_t[PAYLOAD_MAXLEN];
    uint32_t clock_rate = 90000 / 30;
    uint32_t timestamp  = 0;

    /* If you don't want uvgRTP to handle timestamping but wish to do that yourself
     * AND you want to use RTCP, timestamping info must be provided for the RTCP so
     * it is able calculate sensible values for synchronization info
     *
     * The first parameter is NTP time associated with the corresponding RTP timestamp,
     * second parameter is clock rate and the third parameter is RTP timestamp for t = 0
     * (it can be zero or some random number, does not matter) */
    hevc->get_rtcp()->set_ts_info(uvgrtp::clock::ntp::now(), clock_rate, timestamp);

    for (int i = 0; i < 10; ++i) {
        /* The timestamp is given as the third parameter and it should be advanced
         * in accordance with the media stream clock rate. For example, for HEVC, the clock rate is 90000. */
        if (hevc->push_frame(buffer, PAYLOAD_MAXLEN, clock_rate * timestamp++, RTP_NO_FLAGS) != RTP_OK)
            fprintf(stderr, "Failed to send RTP frame!");
    }

    /* Session must be destroyed manually */
    delete[] buffer;
    ctx.destroy_session(sess);

    return 0;
}
