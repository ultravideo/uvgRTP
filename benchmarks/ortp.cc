#include <ortp/ortp.h>
#include <csignal>
#include <cstdlib>

#include <cstring>
#include <cstdint>
#include <chrono>
#include <cstdio>

extern void *get_mem(int argc, char **argv, size_t& len);

int main(int argc, char *argv[])
{
	RtpSession *session;
	uint32_t user_ts = 0;

	ortp_init();
	ortp_scheduler_init();
	ortp_set_log_level_mask(NULL, ORTP_MESSAGE | ORTP_WARNING | ORTP_ERROR);
	session = rtp_session_new(RTP_SESSION_SENDONLY);	
	
	rtp_session_set_scheduling_mode(session, 1);
	rtp_session_set_blocking_mode(session, 1);
	rtp_session_set_connected_mode(session, TRUE);
	rtp_session_set_remote_addr(session, "127.0.0.1", 8888);
	rtp_session_set_payload_type(session, 96);
	
    size_t len = 0;
    void *mem  = get_mem(argc, argv, len);

    uint64_t chunk_size, total_size;
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

		rtp_session_send_with_ts(session, (uint8_t *)mem + i, chunk_size, 3);

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
