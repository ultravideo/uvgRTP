#include <uvgrtp/lib.hh>

#define PAYLOAD_MAXLEN 100

int main(void)
{
    /* See sending.cc for more details */
    uvg_rtp::context ctx;

    /* See sending.cc for more details */
    uvg_rtp::session *sess = ctx.create_session("127.0.0.1");

    /* See sending.cc for more details */
    uvg_rtp::media_stream *hevc = sess->create_stream(8888, 8889, RTP_FORMAT_H265, 0);

    /* Three buffers that create one discrete frame (perhaps three NAL units) that all should have the same timestamp */
    auto buffer1 = std::unique_ptr<uint8_t[]>(new uint8_t[PAYLOAD_MAXLEN]);
    auto buffer2 = std::unique_ptr<uint8_t[]>(new uint8_t[PAYLOAD_MAXLEN]);
    auto buffer3 = std::unique_ptr<uint8_t[]>(new uint8_t[PAYLOAD_MAXLEN]);

    /* First two calls fragment the input buffer into chunks of 1500 bytes, push them to
     * frame queue but do NOT flush the queue due to "RTP_MORE" flag being present.
     *
     * The last call with "RTP_MORE" missing will fragment the input buffer and flush the frame queue.
     * All fragments have the same timestamp and they appear to the receiving end as a collection of
     * fragments of one full HEVC frame
     *
     * This functionality can be used to support f.ex. HEVC slices */
    (void)hevc->push_frame(std::move(buffer1), PAYLOAD_MAXLEN, RTP_SLICE | RTP_MORE);
    (void)hevc->push_frame(std::move(buffer2), PAYLOAD_MAXLEN, RTP_SLICE | RTP_MORE);
    (void)hevc->push_frame(std::move(buffer3), PAYLOAD_MAXLEN, RTP_SLICE);

    /* Session must be destroyed manually */
    ctx.destroy_session(sess);

    return 0;
}
