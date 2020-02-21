#include <kvzrtp/lib.hh>
#include <kvzrtp/clock.hh>
#include <cstring>
#include<algorithm> 
#include <easy/profiler.h>

#define MAX(a, b) (((int)(a) > (int)(b)) ? (int)(a) : (int)(b))

extern void *get_mem(int argc, char **argv, size_t& len);

size_t bytes_sent     = 0;
size_t bytes_received = 0;

int main(int argc, char **argv)
{
    uint64_t chunk_size = 0;
    uint64_t total_size = 0;
    uint64_t fake_size  = 0;
    uint64_t fpt_us     = 0;
    uint64_t diff       = 0;
    uint64_t frames     = 0;
    rtp_error_t ret     = RTP_OK;

    size_t len          = 0;
    void *mem           = get_mem(argc, argv, len);

    kvz_rtp::context rtp_ctx;

    kvz_rtp::session *sess      = rtp_ctx.create_session("127.0.0.1");
    kvz_rtp::media_stream *hevc = sess->create_stream(8889, 8888, RTP_FORMAT_HEVC, RCE_SYSTEM_CALL_DISPATCHER);

    std::chrono::high_resolution_clock::time_point start, fpt_start, fpt_end, end;

    start = std::chrono::high_resolution_clock::now();

    for (int rounds = 0; rounds < 1; ++rounds) {
        for (size_t offset = 0, k = 0; offset < len; ++k) {
            memcpy(&chunk_size, (uint8_t *)mem + offset, sizeof(uint64_t));

            offset          += sizeof(uint64_t);
            total_size += chunk_size;

            fpt_start = std::chrono::high_resolution_clock::now();

            if ((ret = hevc->push_frame((uint8_t *)mem + offset, chunk_size, 0)) != RTP_OK) {
                fprintf(stderr, "push_frame() failed!\n");
                for (;;);
            }

            fpt_end = std::chrono::high_resolution_clock::now();
            diff = std::chrono::duration_cast<std::chrono::milliseconds>(fpt_end - fpt_start).count();

            std::this_thread::sleep_for(std::chrono::microseconds(4500));

            frames     += 1;
            offset     += chunk_size;
            bytes_sent += chunk_size;
            fake_size  += chunk_size;
            fpt_us     += diff;
        }
    }

    end  = std::chrono::high_resolution_clock::now();
    diff = (uint64_t)std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    fprintf(stderr, "%lu bytes, %lu kB, %lu MB took %lu ms %lu s\n",
        bytes_sent, bytes_sent / 1000, bytes_sent / 1000 / 1000,
        diff, diff / 1000
    );

    rtp_ctx.destroy_session(sess);
}
