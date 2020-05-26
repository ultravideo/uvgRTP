#include <liveMedia/liveMedia.hh>
#include <BasicUsageEnvironment.hh>
#include <GroupsockHelper.hh>
#include "framedsource.hh"
#include "H265VideoStreamDiscreteFramer.hh"

int main(int argc, char **argv)
{
    if (argc != 5) {
        fprintf(stderr, "usage: ./%s <addr> <# of threads> <fps> <mode>\n", __FILE__);
        return -1;
    }

    H265VideoStreamDiscreteFramer *framer;
    H265FramedSource *framedSource;
    TaskScheduler *scheduler;
    UsageEnvironment *env;
    RTPSink *videoSink;

    scheduler = BasicTaskScheduler::createNew();
    env = BasicUsageEnvironment::createNew(*scheduler);

    framedSource = H265FramedSource::createNew(*env, atoi(argv[3]));
    framer = H265VideoStreamDiscreteFramer::createNew(*env, framedSource);

    Port rtpPort(8888);
    struct in_addr dst_addr;
    dst_addr.s_addr = our_inet_addr("127.0.0.1");

    Groupsock rtpGroupsock(*env, dst_addr, rtpPort, 255);

    OutPacketBuffer::maxSize = 1600000;
    videoSink = H265VideoRTPSink::createNew(*env, &rtpGroupsock, 96);

    videoSink->startPlaying(*framer, NULL, videoSink);
    env->taskScheduler().doEventLoop();
}
