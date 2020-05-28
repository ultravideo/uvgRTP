#include <uvgrtp/lib.hh>

#define PAYLOAD_MAXLEN 256

int main(void)
{
    /* See sending.cc for more details */
    uvg_rtp::context rtp_ctx;

    /* start session with remote at ip 10.21.25.2
     * and bind ourselves to interface pointed to by the ip 10.21.25.200 */
    uvg_rtp::session *sess = rtp_ctx.create_session("10.21.25.2", "10.21.25.200");

    /* 8888 is source port or the port for the interface where data is received (ie. 10.21.25.200:8888)
     * 8889 is remote port or the port for the interface where the data is sent (ie. 10.21.25.2:8889) */
    uvg_rtp::media_stream *hevc = sess->create_stream(8888, 8889, RTP_FORMAT_HEVC, 0);

    while (true) {
        std::unique_ptr<uint8_t[]> buffer = std::unique_ptr<uint8_t[]>(new uint8_t[PAYLOAD_MAXLEN]);

        if (hevc->push_frame(std::move(buffer), PAYLOAD_MAXLEN, RTP_NO_FLAGS) != RTP_OK)
            fprintf(stderr, "failed to push hevc frame\n");

        std::this_thread::sleep_for(std::chrono::milliseconds(800));
    }

    sess->destroy_stream(hevc);
    rtp_ctx.destroy_session(sess);
}
