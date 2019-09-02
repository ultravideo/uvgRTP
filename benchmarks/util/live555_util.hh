#pragma once

#include <liveMedia.hh>
#include <FramedSource.hh>
#include <UsageEnvironment.hh>
#include <GroupsockHelper.hh>
#include <BasicUsageEnvironment.hh>
#include <Groupsock.hh>
#include <GroupsockHelper.hh>

#include <cstring>
#include <string>
#include <netinet/ip.h>
#include <arpa/inet.h>

class FramedSourceCustom : public FramedSource
{
public:
  FramedSourceCustom(UsageEnvironment *env);
  ~FramedSourceCustom();

  void send_data(void *mem, size_t len);

  virtual void doGetNextFrame();
protected:

  virtual void doStopGettingFrames();
private:
  void push_hevc_chunk(void *mem, size_t len);
  void sendFrame(void *mem, size_t len);

  EventTriggerId afterEvent_;

  bool separateInput_;
  bool ending_;
  bool removeStartCodes_;

  TaskToken currentTask_;

  bool stop_;
  bool noMoreTasks_;
};

struct Connection
{
    Port *rtpPort;
    Port *rtcpPort;
    Groupsock *rtpGroupsock;
    Groupsock *rtcpGroupsock;
};

void createConnection(UsageEnvironment *env, Connection& conn, std::string sess, std::string ip, uint16_t port);
