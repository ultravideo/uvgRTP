#include <uvgrtp/lib.hh>
#include <uvgrtp/formats/rawvideo.hh>

void hook(void *arg, uvgrtp::frame::rtp_frame *frame)
{
    LOG_INFO("Raw video scan line(s) received");
    uvgrtp::frame::dealloc_frame(frame);
}

int main(void)
{
    /* To use the library, one must create a global RTP context object */
    uvgrtp::context ctx;

    /* Each new IP address requires a separate RTP session */
    uvgrtp::session *sess     = ctx.create_session("127.0.0.1");
    uvgrtp::media_stream *rwv = sess->create_stream(8888, 8888, RTP_FORMAT_RAW_VIDEO, RTP_NO_FLAGS);

    /* install receive hook for asynchronous frame reception */
    rwv->install_receive_hook(nullptr, hook);

    /* specify pixel format of input/output data */
    rwv->configure_ctx(RCC_FMT_SUBTYPE, uvgrtp::formats::RWV_FMT_YUV420);

    for (;;) {
        uint8_t data[36];

        rwv->push_frame((uint8_t *)data, sizeof(data), RTP_NO_FLAGS);
        /* TODO: what kind of data is given to push_frame()? */
    }

    /* Session must be destroyed manually */
    ctx.destroy_session(sess);

    return 0;
}
