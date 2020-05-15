#include <uvgrtp/lib.hh>
#include <uvgrtp/clock.hh>
#include <cstring>
#include <algorithm>

extern void *get_mem(int argc, char **argv, size_t& len);

std::chrono::high_resolution_clock::time_point fpts, fpte;

void hook(void *arg, uvg_rtp::frame::rtp_frame *frame)
{
    (void)arg, (void)frame;

    if (frame)
        fpte = std::chrono::high_resolution_clock::now();
}

int main(void)
{
    size_t len      = 0;
    void *mem       = get_mem(0, NULL, len);
    uint64_t csize  = 0;
    uint64_t diff   = 0;
    size_t frames   = 0;
    size_t total    = 0;
    rtp_error_t ret = RTP_OK;

    uvg_rtp::context rtp_ctx;
    uvg_rtp::frame::rtp_frame *frame;

    auto sess = rtp_ctx.create_session("127.0.0.1");
    auto hevc = sess->create_stream(
        8888,
        8888,
        RTP_FORMAT_HEVC,
        RCE_SYSTEM_CALL_DISPATCHER
    );

    hevc->install_receive_hook(nullptr, hook);

    for (size_t offset = 0; offset < len; ++frames) {
        memcpy(&csize, (uint8_t *)mem + offset, sizeof(uint64_t));

        offset += sizeof(uint64_t);

        fpts = std::chrono::high_resolution_clock::now();

        if ((ret = hevc->push_frame((uint8_t *)mem + offset, csize, 0)) != RTP_OK) {
            fprintf(stderr, "push_frame() failed!\n");
            for (;;);
        }

        /* because the input frame might be split into multiple separate frames, we should
         * calculate the latency using the timestamp before push and after the last received frame.
         *
         * Sleep for 5 seconds before calculating the latency to prevent us from reading the frame
         * receive time too early (NOTE: this does not affect the latency calculations at all) */
        std::this_thread::sleep_for(std::chrono::milliseconds(5000));

        diff = std::chrono::duration_cast<std::chrono::microseconds>(fpte - fpts).count();

        /* If the difference is more than 10 seconds, it's very likely that the frame was dropped
         * and this latency value is bogus and should be discarded */
        if (diff <= 10 * 1000 * 1000)
            total += diff;
        offset += csize;
    }
    rtp_ctx.destroy_session(sess);

    fprintf(stderr, "avg latency: %lf\n", total / (float)frames);

    return 0;
}
