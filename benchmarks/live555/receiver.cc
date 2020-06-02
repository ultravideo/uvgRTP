#include <liveMedia/liveMedia.hh>
#include <BasicUsageEnvironment.hh>
#include <GroupsockHelper.hh>
#include "sink.hh"

int main(int argc, char **argv)
{
    (void)argc, (void)argv;

    TaskScheduler *scheduler = BasicTaskScheduler::createNew();
    UsageEnvironment *env    = BasicUsageEnvironment::createNew(*scheduler);

    Port rtpPort(8888);
    struct in_addr dst_addr;
    dst_addr.s_addr = our_inet_addr("0.0.0.0");
    Groupsock rtpGroupsock(*env, dst_addr, rtpPort, 255);

    OutPacketBuffer::maxSize = 40 * 1000 * 1000;

    RTPSource *source = H265VideoRTPSource::createNew(*env, &rtpGroupsock, 96);
    RTPSink_ *sink    = new RTPSink_(*env);

    sink->startPlaying(*source, nullptr, nullptr);
    env->taskScheduler().doEventLoop();
}
