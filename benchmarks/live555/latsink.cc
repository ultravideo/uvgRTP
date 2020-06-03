#include <chrono>
#include <climits>
#include <thread>
#include <RTPInterface.hh>
#include "latsink.hh"

#define BUFFER_SIZE 40 * 1000 * 1000

static size_t frames = 0;
static size_t bytes  = 0;
static std::chrono::high_resolution_clock::time_point start;
static std::chrono::high_resolution_clock::time_point last;

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

    fprintf(stderr, "got frame\n");

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
