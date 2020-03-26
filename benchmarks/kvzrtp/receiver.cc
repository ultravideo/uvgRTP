#include <kvzrtp/lib.hh>
#include <kvzrtp/clock.hh>
#include <cstring>
#include<algorithm> 
#include <easy/profiler.h>

void hook(void *arg, kvz_rtp::frame::rtp_frame *frame)
{
    (void)arg;

    static size_t pkts = 0;
    static std::chrono::high_resolution_clock::time_point start, end;

    if (pkts == 0)
        start = std::chrono::high_resolution_clock::now();

    (void)kvz_rtp::frame::dealloc_frame(frame);

    if (++pkts == 602) {
        fprintf(stderr, "%lu\n",
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::high_resolution_clock::now() - start
            ).count()
        );
        exit(EXIT_SUCCESS);
    }
}

int main(void)
{
    kvz_rtp::context rtp_ctx;

    kvz_rtp::session *sess      = rtp_ctx.create_session("127.0.0.1");
    kvz_rtp::media_stream *hevc = sess->create_stream(8888, 8889, RTP_FORMAT_HEVC, RCE_SYSTEM_CALL_DISPATCHER);

    hevc->install_receive_hook(NULL, hook);

    for (;;)
        std::this_thread::sleep_for(std::chrono::milliseconds(200));

    rtp_ctx.destroy_session(sess);
}
