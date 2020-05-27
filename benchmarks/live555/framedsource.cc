#include <GroupsockHelper.hh>
#include <FramedSource.hh>
#include "framedsource.hh"
#include <chrono>
#include <queue>
#include <thread>

EventTriggerId H265FramedSource::eventTriggerId = 0;
unsigned H265FramedSource::referenceCount       = 0;

extern void *get_mem(int, char **, size_t&);
extern int get_next_frame_start(uint8_t *, uint32_t, uint32_t, uint8_t&);

uint8_t *buf;
size_t offset    = 0;
size_t bytes     = 0;
uint64_t current = 0;
uint64_t period  = 0;
bool initialized = false;

std::queue<std::pair<size_t, uint8_t *>> nals;
std::chrono::high_resolution_clock::time_point start, end;

static void splitIntoNals(void)
{
    size_t len   = 0;
    uint8_t *mem = (uint8_t *)get_mem(0, NULL, len);

    uint8_t start_len;
    int32_t prev_offset = 0;
    int offset = get_next_frame_start((uint8_t *)mem, 0, len, start_len);
    prev_offset = offset;

    while (offset != -1) {
        offset = get_next_frame_start((uint8_t *)mem, offset, len, start_len);

        if (offset > 4 && offset != -1) {
            nals.push(std::make_pair(offset - prev_offset - start_len, &mem[prev_offset]));
            prev_offset = offset;
        }
    }

    if (prev_offset == -1)
        prev_offset = 0;

    nals.push(std::make_pair(len - prev_offset, &mem[prev_offset]));

    initialized = true;
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

H265FramedSource::~H265FramedSource()
{
    if (!--referenceCount) {
        envir().taskScheduler().deleteEventTrigger(eventTriggerId);
        eventTriggerId = 0;
    }
}

void H265FramedSource::doGetNextFrame()
{
    /* The benchmark has started, start the timer and split the input video into NAL units */
    if (!initialized) {
        splitIntoNals();
        start = std::chrono::high_resolution_clock::now();
    }
    
    if (nals.empty()) {
        end = std::chrono::high_resolution_clock::now();
        uint64_t diff = (uint64_t)std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        fprintf(stderr, "%lu bytes, %lu kB, %lu MB took %lu ms %lu s\n",
            bytes, bytes / 1000, bytes / 1000 / 1000,
            diff, diff / 1000
        );
        exit(1);
    }
    
    uint64_t runtime = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::high_resolution_clock::now() - start
    ).count();

    if (runtime < current * period)
        std::this_thread::sleep_for(std::chrono::microseconds(current * period - runtime));
    
    ++current;
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

    auto nal = nals.front();
    nals.pop();

    uint8_t *newFrameDataStart = nal.second;
    unsigned newFrameSize      = nal.first;

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
