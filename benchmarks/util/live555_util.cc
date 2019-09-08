#include <chrono>
#include <mutex>
#include <thread>

#include "live555_util.hh"

#define MAX_WRITE_SIZE 1444

extern int get_next_frame_start(uint8_t *data, uint32_t offset, uint32_t data_len, uint8_t& start_len);

FramedSourceCustom::FramedSourceCustom(UsageEnvironment *env)
    :FramedSource(*env)
{
    len_ = 0;
    off_ = 0;
    chunk_ptr_ = 0;
    afterEvent_ = envir().taskScheduler().createEventTrigger((TaskFunc*)FramedSource::afterGetting);
}

FramedSourceCustom::~FramedSourceCustom()
{
}

void FramedSourceCustom::doGetNextFrame()
{
    if (isCurrentlyAwaitingData())
        sendFrame();
}

void FramedSourceCustom::doStopGettingFrames()
{
    noMoreTasks_ = true;
}

void FramedSourceCustom::splitIntoNals()
{
    uint8_t start_len;
    int32_t prev_offset = 0;
    int offset = get_next_frame_start((uint8_t *)c_chunk_, 0, c_chunk_len_, start_len);
    prev_offset = offset;

    while (offset != -1) {
        offset = get_next_frame_start((uint8_t *)c_chunk_, offset, c_chunk_len_, start_len);

        if (offset > 4 && offset != -1) {
            nals_.push(std::make_pair(offset - prev_offset - start_len, &c_chunk_[prev_offset]));
            prev_offset = offset;
        }
    }

    if (prev_offset == -1)
        prev_offset = 0;

    nals_.push(std::make_pair(c_chunk_len_ - prev_offset, &c_chunk_[prev_offset]));
}

void FramedSourceCustom::sendFrame()
{
    /* initialization is not ready but scheduler
     * is already asking for frames, send empty frame */
    if (len_ == 0 && off_ == 0) {
        fFrameSize = 0;
        envir().taskScheduler().triggerEvent(afterEvent_, this);
        return;
    }

    if (c_chunk_ == nullptr) {
        fpt_start_ = std::chrono::high_resolution_clock::now();

        if (chunks_.empty()) {
            printStats();
        }

        auto cinfo   = chunks_.front();
        chunks_.pop();

        c_chunk_     = cinfo.second;
        c_chunk_len_ = cinfo.first;
        c_chunk_off_ = 0;

        splitIntoNals();
    }

    if (c_nal_ == nullptr) {
        auto ninfo = nals_.front();
        nals_.pop();

        c_nal_     = ninfo.second;
        c_nal_len_ = ninfo.first;
        c_nal_off_ = 0;
    }

    void  *send_ptr = nullptr;
    size_t send_len = 0;
    size_t send_off = 0;

    if (c_nal_len_ < MAX_WRITE_SIZE) {
        send_len = c_nal_len_;
        send_ptr = c_nal_;
        send_off = 0;
    } else {
        int left = c_nal_len_ - c_nal_off_;

        if (left < MAX_WRITE_SIZE)
            send_len = left;
        else
            send_len = MAX_WRITE_SIZE;

        send_ptr = c_nal_;
        send_off = c_nal_off_;
    }

    memcpy(fTo, (uint8_t *)send_ptr + send_off, send_len);
    fFrameSize = send_len;
    afterGetting(this);

    /* check if we need to change chunk or nal unit */
    bool nal_written_fully = (c_nal_len_ <= c_nal_off_ + send_len);

    if (nal_written_fully && nals_.empty()) {
        c_chunk_ = nullptr;

        n_calls_++;
        fpt_end_ = std::chrono::high_resolution_clock::now();
        diff_total_ += std::chrono::duration_cast<std::chrono::microseconds>(fpt_end_ - fpt_start_).count();
    } else {
        if (!nal_written_fully) {
            c_nal_off_ += send_len;
        } else {
            c_nal_ = nullptr;
        }
    }
}

void FramedSourceCustom::printStats()
{
    *stop_ = 1;
    end_   = std::chrono::high_resolution_clock::now();

    uint64_t diff = (uint64_t)std::chrono::duration_cast<std::chrono::milliseconds>(end_ - start_).count();

    fprintf(stderr, "%lu bytes, %lu kB, %lu MB took %lu ms %lu s\n",
        total_size_, total_size_ / 1000, total_size_ / 1000 / 1000,
        diff, diff / 1000
    );

    fprintf(stderr, "n calls %u\n", n_calls_);
    fprintf(stderr, "avg processing time of frame: %lu\n", diff_total_ / n_calls_);
}

void FramedSourceCustom::startFramedSource(void *mem, size_t len, char *stop_rtp)
{
    mem_        = mem;
    len_        = len;
    off_        = 0;
    n_calls_    = 0;
    diff_total_ = 0;
    stop_       = stop_rtp;

    c_chunk_     = nullptr;
    c_chunk_len_ = 0;
    c_chunk_off_ = 0;
    total_size_  = 0;

    uint64_t chunk_size = 0;

    for (size_t i = 0, k = 0; i < len && k < 3000; k++) {
        memcpy(&chunk_size, (uint8_t *)mem + i, sizeof(uint64_t));

        i += sizeof(uint64_t);

        chunks_.push(std::make_pair(chunk_size, (uint8_t *)mem_ + i));

        i += chunk_size;
        total_size_ += chunk_size;
    }

    start_ = std::chrono::high_resolution_clock::now();
}

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

