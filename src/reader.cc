#include <cstring>
#include <iostream>

#include "reader.hh"

static int frameReceiver(RTPReader *reader)
{
    sockaddr_in fromAddr;

    std::cerr << "starting to listen to address and port" << std::endl;

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
            const uint8_t *inBuffer = reader->getInPacketBuffer();

            RTPGeneric::GenericFrame *frame = RTPGeneric::createGenericFrame();

            frame->marker      = (inBuffer[1] & 0x80) ? 1 : 0;
            frame->rtp_payload = (inBuffer[1] & 0x7f);

            frame->rtp_sequence  = ntohs(*(uint16_t *)&inBuffer[2]);
            frame->rtp_timestamp = ntohl(*(uint32_t *)&inBuffer[4]);
            frame->rtp_ssrc      = ntohl(*(uint32_t *)&inBuffer[8]);
            frame->data          = new uint8_t[ret - 12];
            frame->dataLen       = ret - 12;

            memcpy(frame->data, &inBuffer[12], frame->dataLen);

            reader->addOutgoingFrame(frame);
        }
    }

    std::cerr << "THREAD EXITING!!" << std::endl;
}

RTPReader::RTPReader(std::string srcAddr, int srcPort):
    RTPConnection(true),
    srcAddr_(srcAddr),
    srcPort_(srcPort),
    active_(false)
{
}

RTPReader::~RTPReader()
{
    active_ = false;

    // TODO how to stop thread???
    //      private global variable set here from true to false and thread exist???
    //
    /* runner_.std::thread::~thread(); */

    delete inPacketBuffer_;
    /* delete runner_; */
}

int RTPReader::start()
{
    // TODO open socket
    if ((socket_ = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("RTPConnection::open");
        return RTP_SOCKET_ERROR;
    }

    sockaddr_in addrIn_;

    memset(&addrIn_, 0, sizeof(addrIn_));
    addrIn_.sin_family = AF_INET;  
    addrIn_.sin_addr.s_addr = htonl(INADDR_ANY);
    addrIn_.sin_port = htons(srcPort_);

    if (bind(socket_, (struct sockaddr *) &addrIn_, sizeof(addrIn_)) < 0) {
        perror("RTPConnection::open");
        return RTP_BIND_ERROR;
    }

    inPacketBuffer_ = new uint8_t[MAX_PACKET];
    inPacketBufferLen_ = MAX_PACKET;

    active_ = true;
    id_     = rtpGetUniqueId();

    runner_ = new std::thread(frameReceiver, this);
    runner_->detach();

    return 0;
}

RTPGeneric::GenericFrame *RTPReader::pullFrame()
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

void RTPReader::addOutgoingFrame(RTPGeneric::GenericFrame *frame)
{
    if (!frame)
        return;

    framesOut_.push_back(frame);
}
