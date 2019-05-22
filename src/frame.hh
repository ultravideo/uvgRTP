#pragma once

#include "util.hh"

#define INVALID_FRAME_TYPE(ft) (ft < FRAME_TYPE_GENERIC || ft > FRAME_TYPE_HEVC_FU)

namespace RTPFrame {

    enum HEADER_SIZES {
        HEADER_SIZE_RTP      = 12,
        HEADER_SIZE_OPUS     =  1,
        HEADER_SIZE_HEVC_RTP =  2,
        HEADER_SIZE_HEVC_FU  =  1,
    };

    typedef enum FRAME_TYPE {
        FRAME_TYPE_GENERIC = 0, // payload length + RTP Header size (N + 12)
        FRAME_TYPE_OPUS    = 1, // payload length + RTP Header size + Opus header (N + 12 + 0 [for now])
        FRAME_TYPE_HEVC_FU = 2, // payload length + RTP Header size + HEVC RTP Header + FU Header (N + 12 + 2 + 1)
    } frame_type_t;

    struct Frame {
        uint32_t timestamp;
        uint32_t ssrc;
        uint16_t seq;
        uint8_t  payload;
        uint8_t  marker;

        uint8_t *header;
        size_t headerLen;

        uint8_t *data;
        size_t dataLen;

        rtp_format_t rtpFormat;
        frame_type_t frameType;
    };

    /* TODO:  */
    RTPFrame::Frame *allocFrame(size_t payloadLen, frame_type_t type);

    /* TODO:  */
    int deallocFrame(RTPFrame::Frame *frame);

    /* TODO:  */
    int sendFrame(RTPConnection *conn, RTPFrame::Frame *frame);

    /* get pointer to RTP Header or nullptr if frame is invalid */
    uint8_t *getRTPHeader(RTPFrame::Frame *frame)
    {
        if (!frame || INVALID_FRAME_TYPE(frame->frameType))
            return nullptr;

        return frame->header;
    }

    /*  */
    uint8_t *getOpusHeader(RTPFrame::Frame *frame)
    {
        if (!frame || !frame->header || frame->frameType != FRAME_TYPE_OPUS)
            return nullptr;

        return frame->header + HEADER_SIZE_RTP;
    }

    uint8_t *getHEVCRTPHeader(RTPFrame::Frame *frame)
    {
        if (!frame || !frame->header || frame->frameType != FRAME_TYPE_HEVC_FU)
            return nullptr;

        return frame->header + HEADER_SIZE_RTP;
    }

    uint8_t *getHEVCFUHeader(RTPFrame::Frame *frame)
    {
        if (!frame || !frame->header || frame->frameType != FRAME_TYPE_HEVC_FU)
            return nullptr;

        return frame->header + HEADER_SIZE_RTP + HEADER_SIZE_HEVC_RTP;
    }
};
