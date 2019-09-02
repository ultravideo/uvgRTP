#include <cstring>
#include <iostream>
#include <chrono>
#include <ccrtp/rtp.h>

using namespace ost;
using namespace std;

extern void *get_mem(int argc, char **argv, size_t& len);
extern int get_next_frame_start(uint8_t *data, uint32_t offset, uint32_t data_len, uint8_t& start_len);

#define MAX_WRITE_SIZE 1444

int push_hevc_frame(RTPSession& session, uint8_t *mem, size_t chunk_size)
{
    int status;

    if (chunk_size < MAX_WRITE_SIZE) {
        session.putData(0, (uint8_t *)mem, chunk_size);
        return 0;
    }

    for (size_t k = 0; k < chunk_size; k += MAX_WRITE_SIZE) {
        size_t write_size = MAX_WRITE_SIZE;

        if (chunk_size - k < MAX_WRITE_SIZE)
            write_size = chunk_size - k;

        session.putData(0, (uint8_t *)mem + k, write_size);
    }
}

int main(int argc, char **argv)
{
    size_t len = 0;
    void *mem  = get_mem(argc, argv, len);

    RTPSession s(InetHostAddress("127.0.0.1"), 8889);  // bind reception socket

     // Initialization
     s.addDestination(InetHostAddress("127.0.0.1"), 8888); // set one destination for packets
     s.setPayloadFormat(DynamicPayloadFormat(96, 90000));
     /* s.setPayloadFormat(96); */
     s.startRunning(); // start running the packet queue scheduler

    uint64_t chunk_size, total_size;
    uint64_t fpt_ms = 0;
    uint64_t fsize  = 0;
    uint32_t frames = 0;
    std::chrono::high_resolution_clock::time_point start, fpt_start, fpt_end, end;
    start = std::chrono::high_resolution_clock::now();

    for (size_t i = 0; i < len; ) {
        memcpy(&chunk_size, (uint8_t *)mem + i, sizeof(uint64_t));

        i          += sizeof(uint64_t);
        total_size += chunk_size;

        fpt_start = std::chrono::high_resolution_clock::now();

        /* Find nal units,  */
        uint8_t start_len = 0;
        int offset        = get_next_frame_start((uint8_t *)mem, 0, chunk_size, start_len);
        int prev_offset   = offset;

        while (offset != -1) {
            offset = get_next_frame_start((uint8_t *)mem, offset, chunk_size, start_len);

            if (offset > 4) {
                push_hevc_frame(s, (uint8_t *)mem + prev_offset, chunk_size - prev_offset);
                prev_offset = offset;
            }
        }

        if (prev_offset == -1)
            prev_offset = 0;

        push_hevc_frame(s, (uint8_t *)mem + prev_offset, chunk_size - prev_offset);

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

    return 0;

}
