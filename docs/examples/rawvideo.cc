#include <uvgrtp/lib.hh>
#include <uvgrtp/formats/rawvideo.hh>

#define PAYLOAD_MAXLEN 4096

int main(void)
{
    uvgrtp::context ctx;

    uvgrtp::session *sess     = ctx.create_session("127.0.0.1");
    uvgrtp::media_stream *rwv = sess->create_stream(8888, 8889, RTP_FORMAT_RAW_VIDEO, RTP_NO_FLAGS);

    uvgrtp::formats::rwv_config conf = {
        .pixfmt      = uvgrtp::formats::RWV_FMT_YUV420,
        .progressive = false, /* progressive */
        .width       = 640,
        .height      = 480,
        .depth       = 8      /* bit depth */
    };

    rwv->configure_ctx(&conf);

    for (;;) {
        auto buffer = std::unique_ptr<uint8_t[]>(new uint8_t[PAYLOAD_MAXLEN]);

        if (rwv->push_frame(std::move(buffer), PAYLOAD_MAXLEN, RTP_NO_FLAGS) != RTP_OK)
            fprintf(stderr, "Failed to send RTP frame!");
    }

    /* Session must be destroyed manually */
    ctx.destroy_session(sess);

    return 0;
}
