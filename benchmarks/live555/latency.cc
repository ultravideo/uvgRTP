#include <liveMedia/liveMedia.hh>
#include <BasicUsageEnvironment.hh>
#include <GroupsockHelper.hh>
#include "latsink.hh"
#include "latsource.hh"

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

    H265VideoStreamDiscreteFramer *framer;
    H265LatencyFramedSource *framedSource;
    TaskScheduler *scheduler;
    UsageEnvironment *env;
    RTPSink *videoSink;

    scheduler = BasicTaskScheduler::createNew();
    env       = BasicUsageEnvironment::createNew(*scheduler);

    framedSource = H265LatencyFramedSource::createNew(*env);
    framer       = H265VideoStreamDiscreteFramer::createNew(*env, framedSource);

    Port rtpPort(8888);
    struct in_addr dst_addr;
    dst_addr.s_addr = our_inet_addr("10.21.25.2");

    Groupsock rtpGroupsock(*env, dst_addr, rtpPort, 255);

    OutPacketBuffer::maxSize = 40 * 1000 * 1000;
    videoSink = H265VideoRTPSink::createNew(*env, &rtpGroupsock, 96);

    videoSink->startPlaying(*framer, NULL, videoSink);
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
