#include "debug.hh"
#include "frame.hh"
#include "send.hh"
#include "util.hh"

RTPFrame::Frame *RTPFrame::allocFrame(size_t payloadLen, frame_type_t type)
{
    if (payloadLen == 0 || !(type <= FRAME_TYPE_HEVC_FU && type >= FRAME_TYPE_GENERIC)) {
        LOG_ERROR("Invalid parameter!");
        return nullptr;
    }

    LOG_DEBUG("Allocating frame. Size %zu, Type %u", payloadLen, type);

    RTPFrame::Frame *frame = new RTPFrame::Frame;
    size_t headerLen       = 0;

    switch (type) {
        case FRAME_TYPE_GENERIC:
            headerLen = RTP_HEADER_SIZE;
            break;

        /* for now, opus header is not used */
        case FRAME_TYPE_OPUS:
            headerLen = RTP_HEADER_SIZE + 0;
            break;

        case FRAME_TYPE_HEVC_FU:
            headerLen = RTP_HEADER_SIZE + HEVC_RTP_HEADER_SIZE + HEVC_FU_HEADER_SIZE;
            break;
    }

    if (!frame)
        return nullptr;

    if ((frame->start = new uint8_t[payloadLen + headerLen]) == nullptr) {
        delete frame;
        return nullptr;
    }

    frame->headerLen = headerLen;
    frame->dataLen   = payloadLen;
    frame->data      = frame->start + headerLen;

    return frame;
}

int RTPFrame::deallocFrame(RTPFrame::Frame *frame)
{
    if (!frame)
        return RTP_INVALID_VALUE;

    if (frame->start)
        delete frame->start;

    delete frame;
    return RTP_OK;
}

int RTPFrame::sendFrame(RTPConnection *conn, RTPFrame::Frame *frame)
{
    if (!frame)
        return RTP_INVALID_VALUE;

    int ret;

    if ((ret = RTPSender::writeGenericHeader(conn, frame->start, frame->headerLen)) != RTP_OK) {
        LOG_ERROR("Failed to send header! Size %zu, Type %d", frame->headerLen, frame->frameType);
        return ret;
    }

    if ((ret = RTPSender::writePayload(conn, frame->data, frame->dataLen)) != RTP_OK) {
        LOG_ERROR("Failed to send payload! Size %zu, Type %d", frame->dataLen, frame->frameType);
        return ret;
    }

    return RTP_OK;
}
