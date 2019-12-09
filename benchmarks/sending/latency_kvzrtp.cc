#include <kvzrtp/lib.hh>
#include <kvzrtp/clock.hh>
#include <cstring>
#include<algorithm> 
#include <easy/profiler.h>

extern void *get_mem(int argc, char **argv, size_t& len);

std::chrono::high_resolution_clock::time_point latency_start, latency_end;

void recv_hook(void *arg, kvz_rtp::frame::rtp_frame *frame)
{
    latency_end = std::chrono::high_resolution_clock::now();
    uint64_t diff = std::chrono::duration_cast<std::chrono::microseconds>(latency_end - latency_start).count();

    fprintf(stderr, "%u + ", diff);
    exit(EXIT_SUCCESS);
}

int main(int argc, char **argv)
{
    size_t len = 0;
    void *mem  = get_mem(argc, argv, len);

    kvz_rtp::context rtp_ctx;

#if 1
    kvz_rtp::writer *writer1 = rtp_ctx.create_writer("127.0.0.1", 8888, RTP_FORMAT_HEVC);
    kvz_rtp::reader *reader1 = rtp_ctx.create_reader("127.0.0.1", 8888, RTP_FORMAT_HEVC);

#else
    kvz_rtp::writer *writer1 = rtp_ctx.create_writer("10.21.25.2", 8888, RTP_FORMAT_HEVC);
    kvz_rtp::reader *reader1 = rtp_ctx.create_reader("10.21.25.200", 8889, RTP_FORMAT_HEVC);

#endif
    reader1->install_recv_hook(NULL, recv_hook);

    (void)reader1->start();
    (void)writer1->start();

    rtp_error_t ret;
    uint64_t chunk_size = 1000, i = 0, sent = 0;

    memcpy(&chunk_size, (uint8_t *)mem + i, sizeof(uint64_t));
    i += sizeof(uint64_t) + chunk_size;
    memcpy(&chunk_size, (uint8_t *)mem + i, sizeof(uint64_t));
    i += sizeof(uint64_t);

    latency_start = std::chrono::high_resolution_clock::now();
    if ((ret = writer1->push_frame((uint8_t *)mem + i, chunk_size, 0, sent)) != RTP_OK) {
        fprintf(stderr, "push_frame failed!\n");
        for (;;);
    }

    while (true) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1000000));
    }
}
