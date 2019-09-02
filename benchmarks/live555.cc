#include "util/live555_util.hh"

#include <thread>

extern void *get_mem(int argc, char **argv, size_t& len);
extern int get_next_frame_start(uint8_t *data, uint32_t offset, uint32_t data_len, uint8_t& start_len);

void framedsourcefunction(UsageEnvironment *env, char *stop_rtp, void *mem, size_t len)
{
    Connection conn;
    createConnection(env, conn, "0.0.0.0", "127.0.0.1", 8888);

    auto filter = new FramedSourceCustom(env);
    auto sink   = H265VideoRTPSink::createNew(*env, conn.rtpGroupsock, 8888);
    auto source = H265VideoStreamDiscreteFramer::createNew(*env, filter);

    if (!sink->startPlaying(*source, nullptr, nullptr)) {
        fprintf(stderr, "failed to start sink!\n");
    }

    filter->send_data(mem, len);
    *stop_rtp = 1;
}

int main(int argc, char **argv)
{
    size_t len            = 0;
    void *mem             = get_mem(argc, argv, len);
    TaskScheduler *sched  = nullptr;
    UsageEnvironment *env = nullptr;
    char stop_rtp         = 0;

    if ((sched = BasicTaskScheduler::createNew()) == nullptr) {
        fprintf(stderr, "failed to allocate TaskScheduler\n");
        return -1;
    }

    if ((env = BasicUsageEnvironment::createNew(*sched)) == nullptr) {
        fprintf(stderr, "failed to allocate UsageEnvironment\n");
        return -1;
    }

    auto fsf_thread = new std::thread(framedsourcefunction, env, &stop_rtp, mem, len);

    env->taskScheduler().doEventLoop(&stop_rtp);
}
