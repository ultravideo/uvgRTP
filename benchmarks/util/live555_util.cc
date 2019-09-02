#include "live555_util.hh"

FramedSourceCustom::FramedSourceCustom(UsageEnvironment *env)
    :FramedSource(*env)
{
    afterEvent_ = envir().taskScheduler().createEventTrigger((TaskFunc*)FramedSource::afterGetting);
}

FramedSourceCustom::~FramedSourceCustom()
{
}

void FramedSourceCustom::doGetNextFrame()
{
    if(isCurrentlyAwaitingData())
    {
        /* std::unique_ptr<Data> currentFrame = getInput(); */
        /* copyFrameToBuffer(std::move(currentFrame)); */
        envir().taskScheduler().scheduleDelayedTask(1, (TaskFunc*)FramedSource::afterGetting, this);
    }
    else
    {
        fFrameSize = 0;
        currentTask_ = envir().taskScheduler().scheduleDelayedTask(1, (TaskFunc*)FramedSource::afterGetting, this);
    }
}

void FramedSourceCustom::doStopGettingFrames()
{
    fprintf(stderr, "stop gettin frames?\n");
    noMoreTasks_ = true;
}

void FramedSourceCustom::sendFrame()
{
    envir().taskScheduler().triggerEvent(afterEvent_, this);
}

void FramedSourceCustom::send_data(void *mem, size_t len)
{
    fprintf(stderr, "max size: %d\n", fMaxSize);
}

#ifdef _Win
void FramedSourceFilter::copyFrameToBuffer(std::unique_ptr<Data> currentFrame)
{
  fFrameSize = 0;

  if (currentFrame)
  {
    fPresentationTime = currentFrame->presentationTime;
    if (currentFrame->framerate != 0)
    {
      //1000000/framerate is the correct length but
      // 0 works with slices
      fDurationInMicroseconds = 0;
    }

    if (currentFrame->data_size > fMaxSize)
    {
      fFrameSize = fMaxSize;
      fNumTruncatedBytes = currentFrame->data_size - fMaxSize;
      qDebug() << "WARNING, FramedSource : Requested sending larger packet than possible:"
               << currentFrame->data_size << "/" << fMaxSize;
    }
    else
    {
      fFrameSize = currentFrame->data_size;
      fNumTruncatedBytes = 0;
    }

    if (removeStartCodes_ && type_ == HEVCVIDEO)
    {
      fFrameSize -= 4;
      memcpy(fTo, currentFrame->data.get() + 4, fFrameSize);
    }
    else
    {
      memcpy(fTo, currentFrame->data.get(), fFrameSize);
    }

    getStats()->addSendPacket(fFrameSize);
  }
}

void FramedSourceFilter::process()
{
  // There is no way to copy the data here, because the
  // pointer is given only after doGetNextFrame is called
  while(separateInput_)
  {
    framePointerReady_.acquire(1);

    if(stop_)
    {
      return;
    }

    std::unique_ptr<Data> currentFrame = getInput();

    if(currentFrame == nullptr)
    {
      fFrameSize = 0;
      sendFrame();
      return;
    }
    while(currentFrame)
    {
      copyFrameToBuffer(std::move(currentFrame));
      sendFrame();

      currentFrame = nullptr;

      framePointerReady_.acquire(1);
      if(stop_)
      {
        return;
      }
      currentFrame = getInput();
      // copy additional NAL units, if available.
      if(currentFrame == nullptr)
      {
        fFrameSize = 0;
        sendFrame();
        return;
      }
    }
  }
}
#endif

void createConnection(
    UsageEnvironment *env,
    Connection& connection,
    std::string sess_addr,
    std::string ip_addr,
    uint16_t portNum
)
{
    sockaddr_in addr, sess;
    inet_pton(AF_INET, ip_addr.c_str(),   &addr.sin_addr);
    inet_pton(AF_INET, sess_addr.c_str(), &sess.sin_addr);

    connection.rtpPort      = new Port(0);
    connection.rtpGroupsock = new Groupsock(*env, sess.sin_addr, addr.sin_addr, *connection.rtpPort);
    connection.rtpGroupsock->changeDestinationParameters(addr.sin_addr, portNum, 255);
}

