#include <uvgrtp/lib.hh>

#include <climits>
#include <cstring>
#include <iostream>

constexpr int AMOUNT_OF_TEST_PACKETS = 30;
constexpr auto END_WAIT = std::chrono::seconds(5);

void thread_func(void)
{
    /* See sending.cc for more details */
    uvgrtp::context ctx;
    uvgrtp::session *sess = ctx.create_session("127.0.0.1");

    /* Enable SRTP and use ZRTP to manage keys */
    unsigned flags = RCE_SRTP | RCE_SRTP_KMNGMNT_ZRTP;

    /* See sending.cc for more details about create_stream() */
    uvgrtp::media_stream *recv = sess->create_stream(8889, 8888, RTP_FORMAT_GENERIC, flags);

    for (;;) {
        auto frame = recv->pull_frame();
        std::string payload = std::string((char*)frame->payload, frame->payload_len);
        std::cout << "Received SRTP frame. Payload: " << payload << std::endl;

        /* the frame must be destroyed manually */
        (void)uvgrtp::frame::dealloc_frame(frame);
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
    uvgrtp::context ctx;
    uvgrtp::session *sess = ctx.create_session("127.0.0.1");

    /* Enable SRTP and use ZRTP to manage keys */
    unsigned flags = RCE_SRTP | RCE_SRTP_KMNGMNT_ZRTP;

    /* See sending.cc for more details about create_stream() */
    uvgrtp::media_stream *send = sess->create_stream(8888, 8889, RTP_FORMAT_GENERIC, flags);

    char *message  = (char *)"Hello, world!";
    size_t msg_len = strlen(message);

    for (int i = 0; i < AMOUNT_OF_TEST_PACKETS; ++i) {
        uint8_t* message_data = new uint8_t[msg_len];
        memcpy(message_data, message, msg_len);

        send->push_frame((uint8_t *)message_data, msg_len, RTP_NO_FLAGS);
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    std::cout << "All packets sent, waiting " << END_WAIT.count()
        << " seconds before exiting." << std::endl;
    // destroy the session after waiting for END_WAIT seconds
    std::this_thread::sleep_for(END_WAIT);
    sess->destroy_stream(send);
    if (sess) {
        ctx.destroy_session(sess);
    }

    return EXIT_SUCCESS;

}
