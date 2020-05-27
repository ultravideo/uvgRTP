#include <uvgrtp/lib.hh>
#include <uvgrtp/clock.hh>
#include <cstring>
#include <algorithm> 

#define MAX(a, b) (((int)(a) > (int)(b)) ? (int)(a) : (int)(b))

extern void *get_mem(int argc, char **argv, size_t& len);

std::atomic<int> nready(0);

void thread_func(void *mem, size_t len, char *addr, int thread_num, double fps, bool strict)
{
    size_t bytes_sent   = 0;
    uint64_t chunk_size = 0;
    uint64_t total_size = 0;
    uint64_t diff       = 0;
    rtp_error_t ret     = RTP_OK;
    std::string addr_("10.21.25.26");

    uvg_rtp::context rtp_ctx;

    auto sess = rtp_ctx.create_session(addr_);
    auto hevc = sess->create_stream(
        8889 + thread_num,
        8888 + thread_num,
        RTP_FORMAT_HEVC,
        RCE_SYSTEM_CALL_DISPATCHER
    );

    std::chrono::high_resolution_clock::time_point start = std::chrono::high_resolution_clock::now();

	uint64_t currentFrame = 0;
	uint64_t framePeriod = (uint64_t)((1000 / fps) * 1000);

    for (int rounds = 0; rounds < 1; ++rounds) {
        for (size_t offset = 0, k = 0; offset < len; ++k) {
            memcpy(&chunk_size, (uint8_t *)mem + offset, sizeof(uint64_t));

            offset     += sizeof(uint64_t);
            total_size += chunk_size;

            if ((ret = hevc->push_frame((uint8_t *)mem + offset, chunk_size, 0)) != RTP_OK) {
                fprintf(stderr, "push_frame() failed!\n");
                for (;;);
            }
			
			// how long the benchmark has been running
            std::chrono::high_resolution_clock::time_point runTime = std::chrono::high_resolution_clock::now() - start;
			
			// sleep if we are ahead of schedule. This enforces the maximum fps set for sender
			if (runTime < currentFrame*framePeriod)
			{
				// there is a very small lag with this implementation, but it is not cumulative so it doesn't matter
				std::this_thread::sleep_for(std::chrono::microseconds(currentFrame*framePeriod - runTime));
			}
			
			++currentFrame;
            offset     += chunk_size;
            bytes_sent += chunk_size;
        }
    }
    rtp_ctx.destroy_session(sess);

    std::chrono::high_resolution_clock::time_point end  = std::chrono::high_resolution_clock::now();
    diff = (uint64_t)std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    fprintf(stderr, "%lu bytes, %lu kB, %lu MB took %lu ms %lu s\n",
        bytes_sent, bytes_sent / 1000, bytes_sent / 1000 / 1000,
        diff, diff / 1000
    );

end:
    nready++;
}

int main(int argc, char **argv)
{
    if (argc != 5) {
        fprintf(stderr, "usage: ./%s <remote address> <number of threads> <fps> <mode>\n", __FILE__);
        return -1;
    }

    size_t len   = 0;
    void *mem    = get_mem(0, NULL, len);
    int nthreads = atoi(argv[2]);
    bool strict  = !strcmp(argv[4], "strict");
    std::thread **threads = (std::thread **)malloc(sizeof(std::thread *) * nthreads);

    for (int i = 0; i < nthreads; ++i)
        threads[i] = new std::thread(thread_func, mem, len, argv[1], i * 2, atof(argv[3]), strict);

    while (nready.load() != nthreads)
        std::this_thread::sleep_for(std::chrono::milliseconds(20));

    for (int i = 0; i < nthreads; ++i) {
        threads[i]->join();
        delete threads[i];
    }
    free(threads);
}
