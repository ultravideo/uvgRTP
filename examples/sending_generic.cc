#include <uvgrtp/lib.hh>
#include <climits>

#define PAYLOAD_MAXLEN 4096

int main(void)
{
    /* See sending.cc for more details */
    uvg_rtp::context ctx;

    /* See sending.cc for more details */
    uvg_rtp::session *sess = ctx.create_session("127.0.0.1");

    /* To enable interoperability between RTP libraries, uvgRTP won't fragment generic frames by default.
     *
     * If fragmentation for sender and defragmentation for receiver should be enabled,
     * RCE_FRAGMENT_GENERIC flag must be passed to create_stream()
     *
     * This flag will split the input frame into packets of 1500 bytes and set the marker bit
     * for first and last fragment. When the receiver notices a generic frame with a marker bit
     * set, it knows that the RTP frame is in fact a fragment and when all fragments have been
     * received uvgRTP constructs one full RTP frame from the fragments and returns the frame to user.
     *
     * See sending.cc for more details about create_stream() */
    uvg_rtp::media_stream *send = sess->create_stream(8888, 8889, RTP_FORMAT_GENERIC, RCE_FRAGMENT_GENERIC);
    uvg_rtp::media_stream *recv = sess->create_stream(8889, 8888, RTP_FORMAT_GENERIC, RCE_FRAGMENT_GENERIC);

    /* Notice that PAYLOAD_MAXLEN > MTU (4096 > 1500).
     *
     * uvgRTP fragments all generic input frames that are larger than 1500 and in the receiving end,
     * it will reconstruct the full sent frame from fragments when all fragments have been received */
    auto custom_media = std::unique_ptr<uint8_t[]>(new uint8_t[PAYLOAD_MAXLEN]);

    for (int i = 0; i < PAYLOAD_MAXLEN; ++i)
        custom_media[i] = i % CHAR_MAX;

    if (send->push_frame(std::move(custom_media), PAYLOAD_MAXLEN, RTP_NO_FLAGS) != RTP_OK) {
        fprintf(stderr, "Failed to send frame!\n");
        return -1;
    }

    auto frame = recv->pull_frame();

    /* Verify that all packets were received without corruption */
    for (int i = 0; i < PAYLOAD_MAXLEN; ++i) {
        if (frame->payload[i] != (i % CHAR_MAX))
            fprintf(stderr, "frame was corrupted during transfer!\n");
    }

    /* the frame must be destroyed manually */
    (void)uvg_rtp::frame::dealloc_frame(frame);

    /* Session must be destroyed manually */
    ctx.destroy_session(sess);

    return 0;
}
