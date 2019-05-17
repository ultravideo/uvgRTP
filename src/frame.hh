#pragma once

#include "util.hh"

namespace RTPFrame {
    typedef enum FRAME_TYPE {
        FRAME_TYPE_GENERIC = 0, // payload length + RTP Header size (N + 12)
        FRAME_TYPE_OPUS    = 1, // payload length + RTP Header size + Opus header (N + 12 + 0 [for now])
        FRAME_TYPE_HEVC_FU = 2, // payload length + RTP Header size + HEVC RTP Header + FU Header (N + 12 + 2 + 1)
    } frame_type_t;

    struct Frame {
        uint32_t rtpTimestamp;
        uint32_t rtpSsrc;
        uint16_t rtpSequence;
        uint8_t  rtpPayload;
        uint8_t  marker;

        uint8_t *start;
        uint8_t *data;
        size_t dataLen;
        size_t headerLen;

        rtp_format_t rtpFormat;
        frame_type_t frameType;
    };

    /* TODO:  */
    RTPFrame::Frame *allocFrame(size_t payloadLen, frame_type_t type);

    /* TODO:  */
    int deallocFrame(RTPFrame::Frame *frame);

    /* TODO:  */
    int sendFrame(RTPConnection *conn, RTPFrame::Frame *frame);
};
