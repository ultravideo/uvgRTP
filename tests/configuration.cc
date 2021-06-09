#include <../include/lib.hh>
#include <chrono>
#include <thread>

#define PAYLOAD_MAXLEN 4096

int main(void)
{
    using namespace std::chrono_literals;
  
    /* See sending.cc for more details */
    uvgrtp::context ctx;

    /* See sending.cc for more details */
    uvgrtp::session *sess = ctx.create_session("127.0.0.1");
    
    if(sess == nullptr) return EXIT_FAILURE;

    /* Some of the functionality of uvgRTP can be enabled/disabled using RCE_* flags.
     *
     * For example, here the created MediaStream object has RTCP enabled,
     * does not utilize system call clustering to reduce the possibility of packet dropping
     * and prepends a 4-byte HEVC start code (0x00000001) before each NAL unit */
    unsigned flags =
        RCE_RTCP |                      /* enable RTCP */
        RCE_NO_SYSTEM_CALL_CLUSTERING | /* disable system call clustering */
        RCE_H26X_PREPEND_SC;            /* prepend a start code before each NAL unit */

    uvgrtp::media_stream *hevc = nullptr;
    
    // We have to try different ports
    for(int i = 0; i < 20; i++) {
        hevc = sess->create_stream(12345+i, 32165+i, RTP_FORMAT_H265, flags);
        if(hevc != nullptr) break;
    }
    
    if(hevc == nullptr) {
      return EXIT_FAILURE;
    }

    /* uvgRTP context can also be configured using RCC_* flags
     * These flags do not enable/disable functionality but alter default behaviour of uvgRTP
     *
     * For example, here UDP send/recv buffers are increased to 40MB
     * and frame delay is set 150 milliseconds to allow frames to arrive a little late */
    hevc->configure_ctx(RCC_UDP_RCV_BUF_SIZE, 40 * 1000 * 1000);
    hevc->configure_ctx(RCC_UDP_SND_BUF_SIZE, 40 * 1000 * 1000);
    hevc->configure_ctx(RCC_PKT_MAX_DELAY,                 150);

    std::this_thread::sleep_for(2000ms);

    /* Session must be destroyed manually */
    ctx.destroy_session(sess);

    return EXIT_SUCCESS;
}
