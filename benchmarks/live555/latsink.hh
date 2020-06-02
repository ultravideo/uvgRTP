#include <H265VideoRTPSink.hh>

class RTPLatencySink : public MediaSink
{
    public:
        RTPLatencySink(UsageEnvironment& env);
        virtual ~RTPLatencySink();

        void uninit();

        static void afterGettingFrame(
            void *clientData,
            unsigned frameSize,
            unsigned numTruncatedBytes,
            struct timeval presentationTime,
            unsigned durationInMicroseconds
        );

        void afterGettingFrame(
            unsigned frameSize,
            unsigned numTruncatedBytes,
            struct timeval presentationTime,
            unsigned durationInMicroseconds
        );

    protected:
        void process();

    private:
        virtual Boolean continuePlaying();
        uint8_t *fReceiveBuffer;
};
