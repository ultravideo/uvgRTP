#include <H265VideoRTPSink.hh>

class RTPSink_ : public MediaSink
{
public:
  RTPSink_(UsageEnvironment& env);
  virtual ~RTPSink_();

  void uninit();

  static void afterGettingFrame(void* clientData,
                                unsigned frameSize,
                                unsigned numTruncatedBytes,
                                struct timeval presentationTime,
                                unsigned durationInMicroseconds);

  void afterGettingFrame(unsigned frameSize,
                         unsigned numTruncatedBytes,
                         struct timeval presentationTime,
                         unsigned durationInMicroseconds);
protected:
  void process();

  private:

  virtual Boolean continuePlaying();

  u_int8_t* fReceiveBuffer;

  bool addStartCodes_;
};
