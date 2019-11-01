#include <kvzrtp/lib.hh>
#include <kvzrtp/clock.hh>
#include <cstring>

#include <easy/profiler.h>

extern void *get_mem(int argc, char **argv, size_t& len);

size_t bytes_sent     = 0;
size_t bytes_received = 0;

void runner(kvz_rtp::context *rtp_ctx, int n, void *mem, size_t len)
{
#if 0
    kvz_rtp::writer *writer = rtp_ctx->create_writer("10.21.25.26", 8888 + n, RTP_FORMAT_HEVC);
#else
    kvz_rtp::writer *writer = rtp_ctx->create_writer("127.0.0.1", 8888 + n, RTP_FORMAT_HEVC);
#endif
    (void)writer->start();

    uint64_t chunk_size, total_size;
    rtp_error_t ret;
    uint64_t fpt_us = 0, diff = 0;
    uint32_t frames = 0;
    std::chrono::high_resolution_clock::time_point start, fpt_start, fpt_end, end;
    start = std::chrono::high_resolution_clock::now();

    for (int zzz = 0; zzz < 3; ++zzz) {
        for (size_t i = 0; i < len; ) {
            memcpy(&chunk_size, (uint8_t *)mem + i, sizeof(uint64_t));

            i          += sizeof(uint64_t);
            total_size += chunk_size;

            fpt_start = std::chrono::high_resolution_clock::now();

            if ((ret = writer->push_frame((uint8_t *)mem + i, chunk_size, 0)) != RTP_OK) {
                fprintf(stderr, "push_frame failed!\n");
                for (;;);
            }

            fpt_end = std::chrono::high_resolution_clock::now();
            diff = std::chrono::duration_cast<std::chrono::milliseconds>(fpt_end - fpt_start).count();

            fprintf(stderr, "sleep %ld\n", 8 - diff);
            std::this_thread::sleep_for(std::chrono::milliseconds(8 - diff));
            diff = std::chrono::duration_cast<std::chrono::microseconds>(fpt_end - fpt_start).count();

            i += chunk_size;
            frames++;
            bytes_sent += chunk_size;
            fpt_us += diff;
        }
    }

    rtp_ctx->destroy_writer(writer);

    end = std::chrono::high_resolution_clock::now();

    diff = (uint64_t)std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    fprintf(stderr, "%lu bytes, %lu kB, %lu MB took %lu ms %lu s\n",
        bytes_sent, bytes_sent / 1000, bytes_sent / 1000 / 1000,
        diff, diff / 1000
    );
    fprintf(stderr, "# of frames: %u\n", frames);
    fprintf(stderr, "avg frame size: %lu\n", bytes_sent / frames);
    fprintf(stderr, "avg processing time of frame: %lu us\n", fpt_us / frames);

    /* profiler::dumpBlocksToFile("/home/altonen/work/rtplib/benchmarks/receiving/profiler.prof"); */

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
