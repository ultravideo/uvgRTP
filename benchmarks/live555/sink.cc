#include <chrono>
#include <climits>
#include <thread>
#include <RTPInterface.hh>
#include "sink.hh"

#define BUFFER_SIZE 1600000

size_t frames = 0;
size_t bytes  = 0;
std::chrono::high_resolution_clock::time_point start;
std::chrono::high_resolution_clock::time_point last;

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

RTPSink_::RTPSink_(UsageEnvironment& env):
    MediaSink(env)
{
    fReceiveBuffer = new uint8_t[BUFFER_SIZE];
}

RTPSink_::~RTPSink_()
{
}

void RTPSink_::uninit()
{
    stopPlaying();
    delete fReceiveBuffer;
}

void RTPSink_::afterGettingFrame(
    void *clientData,
    unsigned frameSize,
    unsigned numTruncatedBytes,
    struct timeval presentationTime,
    unsigned durationInMicroseconds
)
{
    ((RTPSink_ *)clientData)->afterGettingFrame(
        frameSize,
        numTruncatedBytes,
        presentationTime,
        durationInMicroseconds
    );
}

void RTPSink_::afterGettingFrame(
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

    if (++frames == 601) {
        fprintf(stderr, "%zu %zu %lu\n", bytes, frames,
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::high_resolution_clock::now() - start
            ).count()
        );
        exit(EXIT_SUCCESS);
    }

    continuePlaying();
}

void RTPSink_::process()
{
}

Boolean RTPSink_::continuePlaying()
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
