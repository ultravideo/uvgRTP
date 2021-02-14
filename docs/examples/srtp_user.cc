#include <uvgrtp/lib.hh>
#include <climits>

#define PAYLOAD_MAXLEN  256
#define KEY_SIZE         16
#define SALT_SIZE        14

/* Key and salt for the SRTP session of sender and receiver
 *
 * NOTE: uvgRTP only supports 128 bit keys and 112 bit salts */
uint8_t key[KEY_SIZE]   = { 0 };
uint8_t salt[SALT_SIZE] = { 0 };

void thread_func(void)
{
    /* See sending.cc for more details */
    uvg_rtp::context ctx;
    uvg_rtp::session *sess = ctx.create_session("127.0.0.1");

    /* Enable SRTP and let user manage keys */
    unsigned flags = RCE_SRTP | RCE_SRTP_KMNGMNT_USER;

    /* See sending.cc for more details about create_stream() */
    uvg_rtp::media_stream *recv = sess->create_stream(8889, 8888, RTP_FORMAT_GENERIC, flags);

    /* Before anything else can be done,
     * add_srtp_ctx() must be called with the SRTP key and salt.
     *
     * All calls to "recv" that try to modify and or/use the newly
     * created media stream before calling add_srtp_ctx() will fail */
    recv->add_srtp_ctx(key, salt);

    for (;;) {
        auto frame = recv->pull_frame();
        fprintf(stderr, "Message: '%s'\n", frame->payload);

        /* the frame must be destroyed manually */
        (void)uvg_rtp::frame::dealloc_frame(frame);
    }
}

int main(void)
{
    /* initialize SRTP key and salt */
    for (int i = 0; i < KEY_SIZE; ++i)
        key[i] = i;

    for (int i = 0; i < SALT_SIZE; ++i)
        salt[i] = i * 2;

    /* Create separate thread for the receiver */
    new std::thread(thread_func);

    /* See sending.cc for more details */
    uvg_rtp::context ctx;
    uvg_rtp::session *sess = ctx.create_session("127.0.0.1");

    /* Enable SRTP and let user manage keys */
    unsigned flags = RCE_SRTP | RCE_SRTP_KMNGMNT_USER;

    /* See sending.cc for more details about create_stream() */
    uvg_rtp::media_stream *send = sess->create_stream(8888, 8889, RTP_FORMAT_GENERIC, flags);

    /* Before anything else can be done,
     * add_srtp_ctx() must be called with the SRTP key and salt.
     *
     * All calls to "send" that try to modify and or/use the newly
     * created media stream before calling add_srtp_ctx() will fail */
    send->add_srtp_ctx(key, salt);

    /* All media is now encrypted/decrypted automatically */
    char *message  = (char *)"Hello, world!";
    size_t msg_len = strlen(message);

    for (;;) {
        send->push_frame((uint8_t *)message, msg_len, RTP_NO_FLAGS);
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
}
