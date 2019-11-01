#include <kvzrtp/lib.hh>
#include <kvzrtp/clock.hh>
#include <cstring>

#include <easy/profiler.h>

extern void *get_mem(int argc, char **argv, size_t& len);

size_t bytes_sent     = 0;
size_t bytes_received = 0;

void recv_hook(void *arg, kvz_rtp::frame::rtp_frame *frame)
{
    (void)arg;

    bytes_received += frame->payload_len;

    fprintf(stderr, "%zu\n", bytes_received);

    (void)kvz_rtp::frame::dealloc_frame(frame);
}

void runner(kvz_rtp::context *rtp_ctx, int n, void *mem, size_t len)
{
    (void)mem, (void)len;

    kvz_rtp::reader *reader = rtp_ctx->create_reader("127.0.0.1", 8888 + n, RTP_FORMAT_HEVC);
    reader->install_recv_hook(NULL, recv_hook);
    (void)reader->start();

    for (;;)
        std::this_thread::sleep_for(std::chrono::milliseconds(10000));
}

int main(int argc, char **argv)
{
    /* EASY_PROFILER_ENABLE; */

    size_t len = 0;
    void *mem  = get_mem(argc, argv, len);

    kvz_rtp::context rtp_ctx;

    for (int i = 0; i < 1; ++i) {
        auto t = new std::thread(runner, &rtp_ctx, i, mem, len);
        t->detach();
    }

    for (;;)
        std::this_thread::sleep_for(std::chrono::milliseconds(10000));
}
