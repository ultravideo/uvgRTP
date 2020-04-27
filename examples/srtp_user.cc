#include <uvgrtp/lib.hh>
#include <climits>

#define PAYLOAD_MAXLEN  256
#define KEY_SIZE         16

/* Key for the SRTP session of sender and receiver */
uint8_t key[KEY_SIZE] = { 0 };

void thread_func(void)
{
    /* See sending.cc for more details */
    uvg_rtp::context ctx;
    uvg_rtp::session *sess = ctx.create_session("127.0.0.1");

    /* Enable SRTP and use ZRTP to manage keys */
    unsigned flags = RCE_SRTP | RCE_SRTP_KMNGMNT_USER;

    /* See sending.cc for more details about create_stream() */
    uvg_rtp::media_stream *recv = sess->create_stream(8889, 8888, RTP_FORMAT_GENERIC, flags);

    send->srtp_add_key(key, KEY_SIZE);

    /* All media is now encrypted/decrypted automatically */
}

int main(void)
{
    /* initialize SRTP key */
    for (int i = 0; i < KEY_SIZE; ++i)
        key[i] = i;

    /* Create separate thread for the receiver */
    new std::thread(thread_func);

    /* See sending.cc for more details */
    uvg_rtp::context ctx;
    uvg_rtp::session *sess = ctx.create_session("127.0.0.1");

    /* Enable SRTP and use ZRTP to manage keys */
    unsigned flags = RCE_SRTP | RCE_SRTP_KMNGMNT_USER;

    /* See sending.cc for more details about create_stream() */
    uvg_rtp::media_stream *send = sess->create_stream(8888, 8889, RTP_FORMAT_GENERIC, flags);

    send->srtp_add_key(key, KEY_SIZE);

    /* All media is now encrypted/decrypted automatically */
}
