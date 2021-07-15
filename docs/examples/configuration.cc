#include <uvgrtp/lib.hh>

#include <iostream>

constexpr char LOCAL_ADDRESS[] = "127.0.0.1";
constexpr uint16_t LOCAL_PORT = 8888;

constexpr char REMOTE_ADDRESS[] = "127.0.0.1";
constexpr uint16_t REMOTE_PORT = 8890;

constexpr int BUFFER_SIZE_MB = 40 * 1000 * 1000;
constexpr int PACKET_MAX_DELAY_MS = 150;

constexpr size_t PAYLOAD_MAXLEN = 4096;
constexpr int SEND_TEST_PACKETS = 1000;


void hook(void *arg, uvgrtp::frame::rtp_frame *frame)
{
    std::cout << "Received frame. Payload size: " << frame->payload_len << std::endl;
    uvgrtp::frame::dealloc_frame(frame);
}

int main(void)
{
    std::cout << "Starting uvgRTP configuration example" << std::endl;

    /* Some of the functionality of uvgRTP can be enabled/disabled using RCE_* flags.
     *
     * For example, here the created MediaStream object has RTCP enabled,
     * does not utilize system call clustering to reduce the possibility of packet dropping */
    unsigned send_flags =
        RCE_RTCP |                      /* enable RTCP */
        RCE_NO_SYSTEM_CALL_CLUSTERING;  /* disable system call clustering */

    /* Prepends a 4-byte HEVC start code (0x00000001) before each NAL unit.
     * This way the stream can be saved into a file and played by a media player */
    unsigned receive_flags =
        RCE_RTCP |                      /* enable RTCP */
        RCE_NO_SYSTEM_CALL_CLUSTERING | /* disable system call clustering */
        RCE_H26X_PREPEND_SC;            /* prepend a start code before each NAL unit */

    /* See sending.cc for more details */
    uvgrtp::context ctx;

    uvgrtp::session *local_session = ctx.create_session(REMOTE_ADDRESS);
    uvgrtp::media_stream *send = local_session->create_stream(LOCAL_PORT, REMOTE_PORT, RTP_FORMAT_H265, send_flags);

    uvgrtp::session *remote_session = ctx.create_session(LOCAL_ADDRESS);
    uvgrtp::media_stream *receive = remote_session->create_stream(REMOTE_PORT, LOCAL_PORT, RTP_FORMAT_H265, receive_flags);

    if (receive)
    {
      /* uvgRTP context can also be configured using RCC_* flags
       * These flags do not enable/disable functionality but alter default behaviour of uvgRTP
       *
       * For example, here UDP receive buffer is increased to BUFFER_SIZE_MB
       * and frame delay is set PACKET_MAX_DELAY_MS to allow frames to arrive a little late */
        receive->configure_ctx(RCC_UDP_RCV_BUF_SIZE, BUFFER_SIZE_MB);
        receive->configure_ctx(RCC_PKT_MAX_DELAY,    PACKET_MAX_DELAY_MS);

        /* install receive hook for asynchronous reception */
        receive->install_receive_hook(nullptr, hook);
    }
    else
    {
      std::cerr << "Failed to install receive hook!" << std::endl;
    }

    if (send)
    {

        /* uvgRTP context can also be configured using RCC_* flags
         * These flags do not enable/disable functionality but alter default behaviour of uvgRTP
         *
         * For example, here UDP receive buffer is increased to BUFFER_SIZE_MB
         * and frame delay is set PACKET_MAX_DELAY_MS to allow frames to arrive a little late */
        send->configure_ctx(RCC_UDP_SND_BUF_SIZE, BUFFER_SIZE_MB);

        for (int i = 0; i < SEND_TEST_PACKETS; ++i)
        {
            auto buffer = std::unique_ptr<uint8_t[]>(new uint8_t[PAYLOAD_MAXLEN]);

            std::cout << "Sending frame " << i + 1 << '/' << SEND_TEST_PACKETS << std::endl;
            if (send->push_frame(std::move(buffer), PAYLOAD_MAXLEN, RTP_NO_FLAGS) != RTP_OK)
                fprintf(stderr, "Failed to send RTP frame!");
        }

        local_session->destroy_stream(send);
    }

    if (receive)
    {
      remote_session->destroy_stream(receive);
    }

    if (local_session)
    {
        /* Session must be destroyed manually */
        ctx.destroy_session(local_session);
    }

    if (remote_session)
    {
        /* Session must be destroyed manually */
        ctx.destroy_session(remote_session);
    }

    return 0;
}
