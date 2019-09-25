#include <ortp/ortp.h>
#include <csignal>
#include <cstdlib>

#include <cstring>
#include <cstdint>
#include <chrono>
#include <cstdio>

extern void *get_mem(int argc, char **argv, size_t& len);
extern int get_next_frame_start(uint8_t *data, uint32_t offset, uint32_t data_len, uint8_t& start_len);

#define MAX_WRITE_SIZE 1441

int push_hevc_nal_unit(RtpSession *session, uint8_t *data, size_t data_len, uint32_t& ts)
{
    uint8_t nalType  = (data[0] >> 1) & 0x3F;
    size_t data_left = data_len;
    size_t data_pos  = 0;
    int status       = 0;

    if (data_len < MAX_WRITE_SIZE) {
		rtp_session_send_with_ts(session, (uint8_t *)data, data_len, ts++);
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

		rtp_session_send_with_ts(session, buffer, sizeof(buffer), ts);

        data_pos  += MAX_WRITE_SIZE;
        data_left -= MAX_WRITE_SIZE;

        /* Clear extra bits */
        buffer[2] = nalType;
    }
    buffer[2] |= (1 << 6); /* set E bit to signal end of data */

    memcpy(&buffer[3], &data[data_pos], data_left);

    rtp_session_send_with_ts(session, buffer, 2 + 1 + data_left, ts++);
    return 0;
}

int push_hevc_chunk(RtpSession *s, uint8_t *data, size_t data_len, uint32_t &ts)
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

int main(int argc, char *argv[])
{
	RtpSession *session;
	uint32_t user_ts = 0;

	ortp_init();
	ortp_scheduler_init();
	ortp_set_log_level_mask(0);
	session = rtp_session_new(RTP_SESSION_SENDONLY);	
	
	rtp_session_set_scheduling_mode(session, FALSE);
	rtp_session_set_blocking_mode(session, FALSE);
	rtp_session_set_connected_mode(session, FALSE);
	rtp_session_set_remote_addr(session, "127.0.0.1", 8888);
	rtp_session_set_payload_type(session, 96);
	
    size_t len = 0;
    void *mem  = get_mem(argc, argv, len);

    uint64_t chunk_size, total_size;
    uint64_t fpt_ms = 0;
    uint64_t fsize  = 0;
    uint32_t frames = 0;
    uint64_t bytes  = 0;
    uint32_t ts     = 0;

    std::chrono::high_resolution_clock::time_point start, fpt_start, fpt_end, end;
    start = std::chrono::high_resolution_clock::now();

    for (size_t i = 0; i < len; ) {
        memcpy(&chunk_size, (uint8_t *)mem + i, sizeof(uint64_t));

        i          += sizeof(uint64_t);
        total_size += chunk_size;

        fpt_start = std::chrono::high_resolution_clock::now();
        push_hevc_chunk(session, (uint8_t *)mem + i, chunk_size, ts);
        fpt_end = std::chrono::high_resolution_clock::now();

        i += chunk_size;
        frames++;
        fsize += chunk_size;
        uint64_t diff = std::chrono::duration_cast<std::chrono::microseconds>(fpt_end - fpt_start).count();
        fpt_ms += diff;
    }
    end = std::chrono::high_resolution_clock::now();

    uint64_t diff = (uint64_t)std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    fprintf(stderr, "%lu bytes, %lu kB, %lu MB took %lu ms %lu s\n",
        fsize, fsize / 1000, fsize / 1000 / 1000,
        diff, diff / 1000
    );
    fprintf(stderr, "# of frames: %u\n", frames);
    fprintf(stderr, "avg frame size: %lu\n", fsize / frames);
    fprintf(stderr, "avg processing time of frame: %lu\n", fpt_ms / frames);

	rtp_session_destroy(session);
	ortp_exit();
	ortp_global_stats_display();

	return 0;
}
