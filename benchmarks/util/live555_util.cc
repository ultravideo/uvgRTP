#include "live555_util.hh"

/* TODO: find what is the true maximum */
#define MAX_WRITE_SIZE 1480

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
    if (isCurrentlyAwaitingData())
    {
        fprintf(stderr, "awaiting for data\n");
        /* std::unique_ptr<Data> currentFrame = getInput(); */
        /* copyFrameToBuffer(std::move(currentFrame)); */
        envir().taskScheduler().scheduleDelayedTask(1, (TaskFunc*)FramedSource::afterGetting, this);
    }
    else
    {
        fprintf(stderr, "not waiting for data\n");
        fFrameSize = 0;
        currentTask_ = envir().taskScheduler().scheduleDelayedTask(1, (TaskFunc*)FramedSource::afterGetting, this);
    }
}

void FramedSourceCustom::doStopGettingFrames()
{
    noMoreTasks_ = true;
}

void FramedSourceCustom::sendFrame(void *mem, size_t len)
{
    fFrameSize = len;
    memcpy(fTo, mem, fFrameSize);

    envir().taskScheduler().triggerEvent(afterEvent_, this);
}

void FramedSourceCustom::push_hevc_frame(void *mem, size_t len)
{
    if (len < MAX_WRITE_SIZE) {
        sendFrame();
        return;
    }

    for (size_t k = 0; k < len; k += MAX_WRITE_SIZE) {
        size_t write_size = MAX_WRITE_SIZE;

        if (chunk_size - k < MAX_WRITE_SIZE)
            write_size = chunk_size - k;

        sendFrame((uint8_t *)mem + k, write_size);
    }
}

void FramedSourceCustom::send_data(void *mem, size_t len)
{
    uint64_t chunk_size, total_size;
    rtp_error_t ret;
    uint64_t fpt_ms = 0;
    uint64_t fsize  = 0;
    uint32_t frames = 0;
    uint64_t bytes  = 0;
    std::chrono::high_resolution_clock::time_point start, fpt_start, fpt_end, end;
    start = std::chrono::high_resolution_clock::now();

    for (size_t i = 0; i < len; ) {
        memcpy(&chunk_size, (uint8_t *)mem + i, sizeof(uint64_t));

        i          += sizeof(uint64_t);
        total_size += chunk_size;

        fpt_start = std::chrono::high_resolution_clock::now();

        push_hevc_chunk((uint8_t *)mem + i, chunk_size);

        fpt_end = std::chrono::high_resolution_clock::now();

        i += chunk_size;
        frames++;
        fsize += chunk_size;
        uint64_t diff = std::chrono::duration_cast<std::chrono::microseconds>(fpt_end - fpt_start).count();
        fpt_ms += diff;
    }
    end = std::chrono::high_resolution_clock::now();

    uint64_t diff = (uint64_t)std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    fprintf(stderr, "%lu bytes, %lu kB, %lu MB took %u ms %u s\n",
        fsize, fsize / 1000, fsize / 1000 / 1000,
        diff, diff / 1000
    );

    fprintf(stderr, "# of frames: %u\n", frames);
    fprintf(stderr, "avg frame size: %lu\n", fsize / frames);
    fprintf(stderr, "avg processing time of frame: %lu\n", fpt_ms / frames);
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

