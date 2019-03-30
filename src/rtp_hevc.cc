#include <iostream>
#include <cstdint>
#include <cstring>

#include "rtp_hevc.hh"
#include "conn.hh"

using RTPGeneric::GenericFrame;

static int nextFrameStart(uint8_t *data, uint32_t offset, uint32_t dataLen, uint8_t& startLen)
{
    uint8_t zeros = 0;
    uint32_t pos = 0;

    while (offset + pos < dataLen) {
        if (zeros >= 2 && data[offset + pos] == 1) {
            startLen = zeros + 1;
            return offset + pos +1;
        }

        if (data[offset + pos] == 0)
            zeros++;
        else
            zeros = 0;

        pos++;
    }

    return -1;
}

static int __pushHevcFrame(RTPConnection *conn, uint8_t *data, uint32_t dataLen, uint32_t timestamp)
{
    int ret             = 0;
    uint32_t bufferlen  = 0;
    uint32_t dataPos    = 0;
    uint32_t dataLeft   = dataLen;
    GenericFrame *frame = nullptr;

    if (dataLen <= MAX_PAYLOAD) {
        std::cerr << "[fRTP] send unfrag size " << dataLen << " type " << (uint32_t)((data[0] >> 1) & 0x3F) << std::endl;
        return RTPGeneric::pushGenericFrame(conn, data, dataLen, 0);
    }

    if ((frame = RTPGeneric::createGenericFrame(MAX_PAYLOAD)) == nullptr)
        return -1;

    conn->fillFrame(frame);
    frame->rtp_format = RTP_FORMAT_HEVC;
    
    std::cerr << "[fRTP] send frag size " << dataLen << std::endl;
    uint8_t nalType = (data[0] >> 1) & 0x3F;

    frame->data[0] = 49 << 1; /* fragmentation unit */
    frame->data[1] = 1; /* TID */

    /* Set the S bit with NAL type */
    frame->data[2] = 1 << 7 | nalType;
    dataPos = 2;
    dataLeft -= 2;

    /* Send full payload data packets */
    while (dataLeft + 3 > MAX_PAYLOAD) {
        memcpy(&frame->data[3], &data[dataPos], MAX_PAYLOAD - 3);

        if ((ret = RTPGeneric::pushGenericFrame(conn, frame)))
            goto end;

        dataPos  += (MAX_PAYLOAD - 3);
        dataLeft -= (MAX_PAYLOAD - 3);

        /* Clear extra bits */
        frame->data[2] = nalType;
    }

    /* Signal end and send the rest of the data */
    frame->data[2] |= 1 << 6;
    memcpy(&frame->data[3], &data[dataPos], dataLeft);

    ret = RTPGeneric::pushGenericFrame(conn, frame->data, dataLeft + 3, 1);

end:
    RTPGeneric::destroyGenericFrame(frame);
    return ret;
}

int RTPHevc::pushHevcFrame(RTPConnection *conn, uint8_t *data, uint32_t dataLen, uint32_t timestamp)
{
    uint8_t startLen;
    uint32_t previousOffset = 0;
    int offset = nextFrameStart(data, 0, dataLen, startLen);
    previousOffset = offset;

    while (offset != -1) {
        offset = nextFrameStart(data, offset, dataLen, startLen);

        if (offset > 4 && offset != -1) {
            if (__pushHevcFrame(conn, &data[previousOffset], offset - previousOffset - startLen, timestamp) == -1)
                return -1;

            previousOffset = offset;
        }
    }

    if (previousOffset == -1)
        previousOffset = 0;

    return __pushHevcFrame(conn, &data[previousOffset], dataLen- previousOffset, timestamp);
}
