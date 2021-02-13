#include <uvgrtp/lib.hh>
#include <climits>
#include <cstring>

void thread_func(void)
{
    /* See sending.cc for more details */
    uvg_rtp::context ctx;
    uvg_rtp::session *sess = ctx.create_session("127.0.0.1");

    /* Enable SRTP and use ZRTP to manage keys */
    unsigned flags = RCE_SRTP | RCE_SRTP_KMNGMNT_ZRTP;

    /* Keys creates using Diffie-Hellman mode */
    uvg_rtp::media_stream *recv1 = sess->create_stream(8889, 8888, RTP_FORMAT_GENERIC, flags);

    /* Keys created using Multistream mode */
    uvg_rtp::media_stream *recv2 = sess->create_stream(7778, 7777, RTP_FORMAT_GENERIC, flags);

    for (;;) {
        auto frame = recv1->pull_frame();
        fprintf(stderr, "Message: '%s'\n", frame->payload);
        (void)uvg_rtp::frame::dealloc_frame(frame);

        frame = recv2->pull_frame();
        fprintf(stderr, "Message: '%s'\n", frame->payload);
        (void)uvg_rtp::frame::dealloc_frame(frame);
    }
}

int main(void)
{
    /* Create separate thread for the receiver
     *
     * Because we're using ZRTP for SRTP key management,
     * the receiver and sender must communicate with each other
     * before the actual media communication starts */
    new std::thread(thread_func);

    /* See sending.cc for more details */
    uvg_rtp::context ctx;
    uvg_rtp::session *sess = ctx.create_session("127.0.0.1");

    /* Enable SRTP and use ZRTP to manage keys */
    unsigned flags = RCE_SRTP | RCE_SRTP_KMNGMNT_ZRTP;

    /* Initialize ZRTP and negotiate the keys used to encrypt the media */
    uvg_rtp::media_stream *send1 = sess->create_stream(8888, 8889, RTP_FORMAT_GENERIC, flags);

    /* The first call to create_stream() creates keys for the session using Diffie-Hellman
     * key exchange and all subsequent calls to create_stream() initialize keys for the
     * stream using Multistream mode */
    uvg_rtp::media_stream *send2 = sess->create_stream(7777, 7778, RTP_FORMAT_GENERIC, flags);

    char *message  = (char *)"Hello, world!";
    size_t msg_len = strlen(message);

    for (;;) {
        send1->push_frame((uint8_t *)message, msg_len, RTP_NO_FLAGS);
        send2->push_frame((uint8_t *)message, msg_len, RTP_NO_FLAGS);
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
}
