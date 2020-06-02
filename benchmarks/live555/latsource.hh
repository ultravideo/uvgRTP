#ifndef __h265_framed_source_h__
#define __h265_framed_source_h__

#include <FramedSource.hh>

class H265LatencyFramedSource : public FramedSource {
    public:
        static H265LatencyFramedSource *createNew(UsageEnvironment& env);
        static EventTriggerId eventTriggerId;

    protected:
        H265LatencyFramedSource(UsageEnvironment& env);
        virtual ~H265LatencyFramedSource();

    private:
        void deliverFrame();
        virtual void doGetNextFrame();
        static void deliverFrame0(void *clientData);

        static unsigned referenceCount;
};

#endif /* __h265_framed_source_h__ */
