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
#include <queue>
#include <chrono>
#include <mutex>

class FramedSourceCustom : public FramedSource
{
public:
    FramedSourceCustom(UsageEnvironment *env);
    ~FramedSourceCustom();

    void startFramedSource(void *mem, size_t len, char *stop_rtp);
    virtual void doGetNextFrame();

protected:
    virtual void doStopGettingFrames();

private:
    void sendFrame();
    void printStats();
    void splitIntoNals();

    EventTriggerId afterEvent_;

    bool separateInput_;
    bool ending_;
    bool removeStartCodes_;

    char *stop_;

    int n_calls_;
    uint64_t diff_total_;

    uint64_t total_size_;

    void *mem_;
    int len_;
    int off_;

    uint8_t *c_nal_;
    size_t c_nal_len_;
    size_t c_nal_off_;
    std::queue<std::pair<size_t, uint8_t *>> nals_;

    uint8_t *c_chunk_;
    size_t c_chunk_len_;
    size_t c_chunk_off_;

    int chunk_ptr_;
    std::queue<std::pair<size_t, uint8_t *>> chunks_;
    std::mutex mutex_;

    TaskToken currentTask_;
    std::chrono::high_resolution_clock::time_point start_, end_, fpt_end_, fpt_start_;
    bool noMoreTasks_;
};

struct Connection {
    Port *rtpPort;
    Port *rtcpPort;
    Groupsock *rtpGroupsock;
    Groupsock *rtcpGroupsock;
};

void createConnection(UsageEnvironment *env, Connection& conn, std::string sess, std::string ip, uint16_t port);
