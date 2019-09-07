#include <kvzrtp/lib.hh>
#include <kvzrtp/clock.hh>
#include <cstring>

using std::chrono::high_resolution_clock;

extern void *get_mem(int argc, char **argv, size_t& len);

int main(int argc, char **argv)
{
    size_t len = 0;
    void *mem  = get_mem(argc, argv, len);

    kvz_rtp::context rtp_ctx;

    kvz_rtp::writer *writer = rtp_ctx.create_writer("127.0.0.1", 8888, RTP_FORMAT_HEVC);
    (void)writer->start();

    uint64_t chunk_size, total_size;
    rtp_error_t ret;
    uint64_t fpt_ms = 0;
    uint64_t fsize  = 0;
    uint32_t frames = 0;
    uint64_t bytes  = 0;
    std::chrono::high_resolution_clock::time_point start, fpt_start, fpt_end, end;
    start = std::chrono::high_resolution_clock::now();

    for (size_t i = 0; i < len; ) {
        memcpy(&chunk_size, (uint8_t *)mem + i, sizeof(uint64_t));

        i          += sizeof(uint64_t);
        total_size += chunk_size;

        fpt_start = std::chrono::high_resolution_clock::now();

        if ((ret = writer->push_frame((uint8_t *)mem + i, chunk_size, 0)) != RTP_OK)
            fprintf(stderr, "push_frame failed!\n");

        fpt_end = std::chrono::high_resolution_clock::now();

        i += chunk_size;
        frames++;
        fsize += chunk_size;
        uint64_t diff = std::chrono::duration_cast<std::chrono::microseconds>(fpt_end - fpt_start).count();
        fpt_ms += diff;
    }
    end = std::chrono::high_resolution_clock::now();

    uint64_t diff = (uint64_t)std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    fprintf(stderr, "%lu bytes, %lu kB, %lu MB took %u ms %u s\n",
        fsize, fsize / 1000, fsize / 1000 / 1000,
        diff, diff / 1000
    );
    fprintf(stderr, "# of frames: %u\n", frames);
    fprintf(stderr, "avg frame size: %lu\n", fsize / frames);
    fprintf(stderr, "avg processing time of frame: %lu\n", fpt_ms / frames);

    rtp_ctx.destroy_writer(writer);
}
