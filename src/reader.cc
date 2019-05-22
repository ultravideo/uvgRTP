#include <cstring>
#include <iostream>

#include "debug.hh"
#include "frame.hh"
#include "reader.hh"

RTPReader::RTPReader(std::string srcAddr, int srcPort):
    RTPConnection(true),
    srcAddr_(srcAddr),
    srcPort_(srcPort),
    active_(false),
    receiveHook_(nullptr),
    receiveHookArg_(nullptr)
{
}

RTPReader::~RTPReader()
{
    active_ = false;
    delete inPacketBuffer_;
}

int RTPReader::start()
{
    LOG_INFO("Starting to listen to port %d", srcPort_);

    if ((socket_ = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        LOG_ERROR("Failed to create socket: %s", strerror(errno));
        return RTP_SOCKET_ERROR;
    }

    int enable = 1;
    if (setsockopt(socket_, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0)
        perror("setsockopt(SO_REUSEADDR) failed");

    sockaddr_in addrIn_;

    memset(&addrIn_, 0, sizeof(addrIn_));
    addrIn_.sin_family = AF_INET;  
    addrIn_.sin_addr.s_addr = htonl(INADDR_ANY);
    addrIn_.sin_port = htons(srcPort_);

    if (bind(socket_, (struct sockaddr *) &addrIn_, sizeof(addrIn_)) < 0) {
        LOG_ERROR("Failed to bind to port: %s", strerror(errno));
        return RTP_BIND_ERROR;
    }

    inPacketBufferLen_ = MAX_PACKET;

    if ((inPacketBuffer_ = new uint8_t[MAX_PACKET]) == nullptr) {
        LOG_ERROR("Failed to allocate buffer for incoming data!");
        inPacketBufferLen_ = 0;
    }

    active_ = true;
    id_     = rtpGetUniqueId();

    runner_ = new std::thread(frameReceiver, this);
    runner_->detach();

    return 0;
}

RTPFrame::Frame *RTPReader::pullFrame()
{
    while (framesOut_.empty()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    framesMtx_.lock();
    auto nextFrame = framesOut_.front();
    framesOut_.erase(framesOut_.begin());
    framesMtx_.unlock();

    return nextFrame;
}

bool RTPReader::active()
{
    return active_;
}

uint8_t *RTPReader::getInPacketBuffer() const
{
    return inPacketBuffer_;
}

uint32_t RTPReader::getInPacketBufferLength() const
{
    return inPacketBufferLen_;
}

void RTPReader::addOutgoingFrame(RTPFrame::Frame *frame)
{
    if (!frame)
        return;

    framesOut_.push_back(frame);
}

bool RTPReader::receiveHookInstalled()
{
    return receiveHook_ != nullptr;
}

void RTPReader::installReceiveHook(void *arg, void (*hook)(void *arg, RTPFrame::Frame *))
{
    if (hook == nullptr)
    {
        LOG_ERROR("Unable to install receive hook, function pointer is nullptr!");
        return;
    }

    receiveHook_ = hook;
    receiveHookArg_ = arg;
}

void RTPReader::receiveHook(RTPFrame::Frame *frame)
{
    if (receiveHook_)
        return receiveHook_(receiveHookArg_, frame);
}

int RTPReader::frameReceiver(RTPReader *reader)
{
    LOG_INFO("frameReceiver starting listening...");

    sockaddr_in fromAddr;

    while (reader->active()) {
        int fromAddrSize = sizeof(fromAddr);
        int32_t ret = recvfrom(reader->getSocket(), reader->getInPacketBuffer(), reader->getInPacketBufferLength(),
                               0, /* no flags */
#ifdef _WIN32
                               (SOCKADDR *)&fromAddr, 
                               &fromAddrSize
#else
                               (struct sockaddr *)&fromAddr,
                               (socklen_t *)&fromAddrSize
#endif
                );

        if (ret == -1) {
#if _WIN32
            int _error = WSAGetLastError();
            if (_error != 10035)
                std::cerr << "Socket error" << _error << std::endl;
#else
            perror("frameReceiver");
#endif
            return -1;
        } else {
            LOG_DEBUG("got %d bytes", ret);

            const uint8_t *inBuffer = reader->getInPacketBuffer();
            RTPFrame::Frame *frame  = RTPFrame::allocFrame(ret - RTP_HEADER_SIZE, RTPFrame::FRAME_TYPE_GENERIC);

            if (!frame) {
                LOG_ERROR("Failed to allocate RTP Frame!");
                continue;
            }

            frame->marker    = (inBuffer[1] & 0x80) ? 1 : 0;
            frame->payload   = (inBuffer[1] & 0x7f);
            frame->seq       = ntohs(*(uint16_t *)&inBuffer[2]);
            frame->timestamp = ntohl(*(uint32_t *)&inBuffer[4]);
            frame->ssrc      = ntohl(*(uint32_t *)&inBuffer[8]);

            if (ret - 12 <= 0) {
                LOG_WARN("Got an invalid payload of size %d", ret);
                continue;
            }

            frame->header    = new uint8_t[ret];
            frame->headerLen = ret;

            if (!frame->data) {
                LOG_ERROR("Failed to allocate buffer for RTP frame!");
                continue;
            }

            memcpy(frame->header, inBuffer, ret);

            if (reader->receiveHookInstalled())
                reader->receiveHook(frame);
            else
                reader->addOutgoingFrame(frame);
        }
    }
    LOG_INFO("FrameReceiver thread exiting...");
}
