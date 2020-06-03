#include <liveMedia/liveMedia.hh>
#include <BasicUsageEnvironment.hh>
#include <GroupsockHelper.hh>
#include <thread>
#include <mutex>
#include "latsink.hh"
#include "latsource.hh"
#include "sink.hh"
#include "source.hh"

static int receiver(char *addr)
{
    (void)addr;

    TaskScheduler *scheduler = BasicTaskScheduler::createNew();
    UsageEnvironment *env    = BasicUsageEnvironment::createNew(*scheduler);

    Port rtpPort(8888);
    struct in_addr dst_addr;
    dst_addr.s_addr = our_inet_addr("0.0.0.0");
    Groupsock rtpGroupsock(*env, dst_addr, rtpPort, 255);

    OutPacketBuffer::maxSize = 40 * 1000 * 1000;

    RTPSource *source   = H265VideoRTPSource::createNew(*env, &rtpGroupsock, 96);
    RTPLatencySink *sink = new RTPLatencySink(*env);

    sink->startPlaying(*source, nullptr, nullptr);
    env->taskScheduler().doEventLoop();

    return 0;
}

static int sender(char *addr)
{
    (void)addr;

    std::mutex lat_mtx;
    H265VideoStreamDiscreteFramer *framer;
    H265LatencyFramedSource *framedSource;
    TaskScheduler *scheduler;
    UsageEnvironment *env;
    RTPSink *videoSink;
    RTPSource *source;
    RTPSink_ *sink;

    scheduler = BasicTaskScheduler::createNew();
    env       = BasicUsageEnvironment::createNew(*scheduler);

    OutPacketBuffer::maxSize = 40 * 1000 * 1000;

    Port send_port(8889);
    struct in_addr dst_addr;
    dst_addr.s_addr = our_inet_addr("127.0.0.1");

    Port recv_port(8888);
    struct in_addr src_addr;
    src_addr.s_addr = our_inet_addr("127.0.0.1");

    Groupsock send_socket(*env, dst_addr, send_port, 255);
    Groupsock recv_socket(*env, src_addr, recv_port, 255);

    /* sender */
    videoSink    = H265VideoRTPSink::createNew(*env, &send_socket, 96);
    framedSource = H265LatencyFramedSource::createNew(*env, lat_mtx);
    framer       = H265VideoStreamDiscreteFramer::createNew(*env, framedSource);

    /* receiver */
    source = H265VideoRTPSource::createNew(*env, &recv_socket, 96);
    sink    = new RTPSink_(*env);

    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    videoSink->startPlaying(*framer, NULL, videoSink);
    sink->startPlaying(*source, nullptr, nullptr);
    env->taskScheduler().doEventLoop();

    return 0;
}

int main(int argc, char **argv)
{
    if (argc != 3) {
        fprintf(stderr, "usage: ./%s <send|recv> <ip>\n", __FILE__);
        exit(EXIT_FAILURE);
    }

    return !strcmp(argv[1], "send") ? sender(argv[2]) : receiver(argv[2]);
}
