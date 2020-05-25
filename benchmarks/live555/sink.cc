#include <RTPInterface.hh>
#include "sink.hh"

const uint32_t BUFFER_SIZE = 1600000;

RTPSink_::RTPSink_(UsageEnvironment& env):
  MediaSink(env),
  addStartCodes_(false)
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
    fReceiveBuffer = nullptr;
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
    static int frame = 0;

    (void)frameSize,        (void)numTruncatedBytes;
    (void)presentationTime, (void)durationInMicroseconds;

    fprintf(stderr, "frame %d received, size %u KB!\n", ++frame, frameSize / 1000);
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
