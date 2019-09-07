#include <cstring>
#include <iostream>
#include <chrono>
#include <thread>
#include <ccrtp/rtp.h>

using namespace ost;
using namespace std;

extern void *get_mem(int argc, char **argv, size_t& len);
extern int get_next_frame_start(uint8_t *data, uint32_t offset, uint32_t data_len, uint8_t& start_len);

#define MAX_WRITE_SIZE 1441

int push_hevc_nal_unit(RTPSession& session, uint8_t *data, size_t data_len, uint32_t& ts)
{
    uint8_t nalType  = (data[0] >> 1) & 0x3F;
    size_t data_left = data_len;
    size_t data_pos  = 0;
    int status       = 0;

    if (data_len < MAX_WRITE_SIZE) {
        session.putData(ts++, data, data_len);
        return 0;
    }

    uint8_t buffer[2 + 1 + MAX_WRITE_SIZE];

    buffer[0]  = 49 << 1;           /* fragmentation unit */
    buffer[1]  = 1;                 /* TID */
    buffer[2] = (1 << 7) | nalType; /* Start bit + NAL type */

    data_pos   = 2;
    data_left -= 2;

    while (data_left > MAX_WRITE_SIZE) {
        memcpy(&buffer[3], &data[data_pos], MAX_WRITE_SIZE);

        session.putData(ts, buffer, sizeof(buffer));

        data_pos  += MAX_WRITE_SIZE;
        data_left -= MAX_WRITE_SIZE;

        /* Clear extra bits */
        buffer[2] = nalType;
    }
    buffer[2] |= (1 << 6); /* set E bit to signal end of data */

    memcpy(&buffer[3], &data[data_pos], data_left);

    session.putData(ts++, buffer, 2 + 1 + data_left);
    return 0;
}


int push_hevc_chunk(RTPSession& s, uint8_t *data, size_t data_len, uint32_t& ts)
{
    uint8_t start_len;
    int32_t prev_offset = 0;
    int offset = get_next_frame_start(data, 0, data_len, start_len);
    prev_offset = offset;

    while (offset != -1) {
        offset = get_next_frame_start(data, offset, data_len, start_len);

        if (offset > 4 && offset != -1) {
            push_hevc_nal_unit(s, &data[prev_offset], offset - prev_offset - start_len, ts);
            prev_offset = offset;
        }
    }

    if (prev_offset == -1)
        prev_offset = 0;

    push_hevc_nal_unit(s, &data[prev_offset], data_len - prev_offset, ts);
}

int push_hevc_chunk__(RTPSession& session, uint8_t *mem, size_t chunk_size, uint32_t& ts)
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

    ts++;
    return 0;
}

int main(int argc, char **argv)
{
    size_t len = 0;
    void *mem  = get_mem(argc, argv, len);

    RTPSession s(InetHostAddress("127.0.0.1"), 3333);  // bind reception socket

     // Initialization
     s.addDestination(InetHostAddress("127.0.0.1"), 8888); // set one destination for packets
     s.setPayloadFormat(DynamicPayloadFormat(96, 90000));
     s.startRunning(); // start running the packet queue scheduler

    uint64_t chunk_size, total_size;
    uint64_t fpt_ms = 0;
    uint64_t fsize  = 0;
    uint32_t frames = 0;
    uint32_t ts     = 0;
    std::chrono::high_resolution_clock::time_point start, fpt_start, fpt_end, end;
    start = std::chrono::high_resolution_clock::now();

    for (size_t i = 0; i < len; ) {
        memcpy(&chunk_size, (uint8_t *)mem + i, sizeof(uint64_t));

        i          += sizeof(uint64_t);
        total_size += chunk_size;

        fpt_start = std::chrono::high_resolution_clock::now();
        push_hevc_chunk(s, (uint8_t *)mem + i, chunk_size, ts);
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
    /* for (;;); */

    return 0;
}
