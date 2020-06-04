#include <BasicUsageEnvironment.hh>
#include <FramedSource.hh>
#include <GroupsockHelper.hh>
#include <liveMedia/liveMedia.hh>
#include <RTPInterface.hh>
#include <chrono>
#include <climits>
#include <mutex>
#include <thread>

#include "latsink.hh"
#include "source.hh"

#define BUFFER_SIZE 40 * 1000 * 1000

EventTriggerId H265FramedSource::eventTriggerId = 0;
unsigned H265FramedSource::referenceCount       = 0;
H265FramedSource *framedSource;

static size_t frames       = 0;
static size_t bytes        = 0;

static uint8_t *nal_ptr = nullptr;
static size_t nal_size  = 0;

static std::mutex lat_mtx;
static std::chrono::high_resolution_clock::time_point start;
static std::chrono::high_resolution_clock::time_point last;

static uint8_t *buf;
static size_t offset    = 0;
/* static size_t bytes     = 0; */
static uint64_t current = 0;
static uint64_t period  = 0;

std::chrono::high_resolution_clock::time_point s_tmr, e_tmr;

static void thread_func(void)
{
    unsigned prev_frames = UINT_MAX;

    while (true) {
        std::this_thread::sleep_for(std::chrono::milliseconds(2000));

        if (prev_frames == frames)
            break;

        prev_frames = frames;
    }

    fprintf(stderr, "%zu %zu %lu\n", bytes, frames,
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::high_resolution_clock::now() - start
        ).count()
    );

    exit(EXIT_FAILURE);
}

RTPLatencySink::RTPLatencySink(UsageEnvironment& env):
    MediaSink(env)
{
    fReceiveBuffer = new uint8_t[BUFFER_SIZE];
}

RTPLatencySink::~RTPLatencySink()
{
}

void RTPLatencySink::uninit()
{
    stopPlaying();
    delete fReceiveBuffer;
}

void RTPLatencySink::afterGettingFrame(
    void *clientData,
    unsigned frameSize,
    unsigned numTruncatedBytes,
    struct timeval presentationTime,
    unsigned durationInMicroseconds
)
{
    ((RTPLatencySink *)clientData)->afterGettingFrame(
        frameSize,
        numTruncatedBytes,
        presentationTime,
        durationInMicroseconds
    );
}

void RTPLatencySink::afterGettingFrame(
    unsigned frameSize,
    unsigned numTruncatedBytes,
    struct timeval presentationTime,
    unsigned durationInMicroseconds
)
{
    (void)frameSize,        (void)numTruncatedBytes;
    (void)presentationTime, (void)durationInMicroseconds;

    /* start loop that monitors activity and if there has been
     * no activity for 2s (same as uvgRTP) the receiver is stopped) */
    if (!frames)
        (void)new std::thread(thread_func);

    //fprintf(stderr, "got frame %zu %zu\n", frames + 1, frameSize);

    nal_ptr  = fReceiveBuffer;
    nal_size = frameSize;

    lat_mtx.unlock();
    framedSource->deliver_frame();

    if (++frames == 602) {
        fprintf(stderr, "%zu %zu %lu\n", bytes, frames,
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::high_resolution_clock::now() - start
            ).count()
        );
        exit(EXIT_SUCCESS);
    }

    continuePlaying();
}

void RTPLatencySink::process()
{
}

Boolean RTPLatencySink::continuePlaying()
{
    if (!fSource)
        return False;

    fSource->getNextFrame(
        fReceiveBuffer,
        BUFFER_SIZE,
        afterGettingFrame,
        this,
        onSourceClosure,
        this
    );

    return True;
}

H265FramedSource *H265FramedSource::createNew(UsageEnvironment& env, unsigned fps)
{
    return new H265FramedSource(env, fps);
}

H265FramedSource::H265FramedSource(UsageEnvironment& env, unsigned fps):
    FramedSource(env),
    fps_(fps)
{
    period = (uint64_t)((1000 / fps) * 1000);

    if (!eventTriggerId)
        eventTriggerId = envir().taskScheduler().createEventTrigger(deliverFrame0);
}

void H265FramedSource::deliver_frame()
{
    deliverFrame();
}

H265FramedSource::~H265FramedSource()
{
    if (!--referenceCount) {
        envir().taskScheduler().deleteEventTrigger(eventTriggerId);
        eventTriggerId = 0;
    }
}

void H265FramedSource::doGetNextFrame()
{
    deliverFrame();
}

void H265FramedSource::deliverFrame0(void *clientData)
{
    ((H265FramedSource *)clientData)->deliverFrame();
}

void H265FramedSource::deliverFrame()
{
    if (!isCurrentlyAwaitingData())
        return;

    if (!lat_mtx.try_lock())
        return;

    uint8_t *newFrameDataStart = nal_ptr;
    unsigned newFrameSize      = nal_size;

    bytes += newFrameSize;

    if (newFrameSize > fMaxSize) {
        fFrameSize = fMaxSize;
        fNumTruncatedBytes = newFrameSize - fMaxSize;
    } else {
        fFrameSize = newFrameSize;
    }

    fDurationInMicroseconds = 0;
    memmove(fTo, newFrameDataStart, fFrameSize);

    FramedSource::afterGetting(this);
}

static int receiver(void)
{
    H265VideoStreamDiscreteFramer *framer;
    TaskScheduler *scheduler;
    UsageEnvironment *env;
    RTPLatencySink *sink;
    struct in_addr addr;
    RTPSink *videoSink;
    RTPSource *source;

    scheduler = BasicTaskScheduler::createNew();
    env       = BasicUsageEnvironment::createNew(*scheduler);

    OutPacketBuffer::maxSize = 40 * 1000 * 1000;
    lat_mtx.lock();

    /* receiver */
    addr.s_addr = our_inet_addr("0.0.0.0");
    Groupsock recv_sock(*env, addr, Port(8888), 255);

    source = H265VideoRTPSource::createNew(*env, &recv_sock, 96);
    sink   = new RTPLatencySink(*env);

    /* sender */
    addr.s_addr = our_inet_addr("10.21.25.200");
    Groupsock send_socket(*env, addr, Port(8889), 255);

    framedSource = H265FramedSource::createNew(*env, 30);
    framer       = H265VideoStreamDiscreteFramer::createNew(*env, framedSource);
    videoSink    = H265VideoRTPSink::createNew(*env, &send_socket, 96);

    videoSink->startPlaying(*framer, NULL, videoSink);
    sink->startPlaying(*source, nullptr, nullptr);
    env->taskScheduler().doEventLoop();

    return 0;
}

int main(int argc, char **argv)
{
    (void)argc, (void)argv;

    return receiver();
}
