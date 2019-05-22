#include <iostream>
#include <cstdint>
#include <cstring>

#include "conn.hh"
#include "debug.hh"
#include "rtp_hevc.hh"
#include "send.hh"

static int __internal_get_next_frame_start(uint8_t *data, uint32_t offset, uint32_t data_len, uint8_t& start_len)
{
    uint8_t zeros = 0;
    uint32_t pos  = 0;

    while (offset + pos < data_len) {
        if (zeros >= 2 && data[offset + pos] == 1) {
            start_len = zeros + 1;
            return offset + pos + 1;
        }

        if (data[offset + pos] == 0)
            zeros++;
        else
            zeros = 0;

        pos++;
    }

    return -1;
}

static rtp_error_t __internal_push_hevc_frame(kvz_rtp::connection *conn, uint8_t *data, uint32_t data_len, uint32_t timestamp)
{
    uint32_t data_pos  = 0;
    uint32_t data_left = data_len;
    rtp_error_t ret    = RTP_OK;

    if (data_len <= MAX_PAYLOAD) {
        LOG_DEBUG("send unfrag size %u, type %u", data_len, (uint32_t)((data[0] >> 1) & 0x3F));
        return kvz_rtp::generic::push_generic_frame(conn, data, data_len, timestamp);
    }

    LOG_DEBUG("send frag size: %u, type %u", data_len, (data[0] >> 1) & 0x3F);

    auto *frame = kvz_rtp::frame::alloc_frame(MAX_PAYLOAD, kvz_rtp::frame::FRAME_TYPE_HEVC_FU);

    if (frame == nullptr) {
        LOG_ERROR("Failed to allocate RTP Frame for HEVC FU payload!");
        return RTP_MEMORY_ERROR;
    }
    frame->rtp_fmt = RTP_FORMAT_HEVC;

    uint8_t *rtp_hdr      = kvz_rtp::frame::get_rtp_header(frame);
    uint8_t *hevc_rtp_hdr = kvz_rtp::frame::get_hevc_rtp_header(frame);
    uint8_t *hevc_fu_hdr  = kvz_rtp::frame::get_hevc_fu_header(frame);
    uint8_t nalType       = (data[0] >> 1) & 0x3F;

    conn->fill_rtp_header(rtp_hdr, timestamp);

    hevc_rtp_hdr[0] = 49 << 1; /* fragmentation unit */
    hevc_rtp_hdr[1] = 1;       /* TID */

    /* Set the S bit with NAL type */
    hevc_fu_hdr[0] = 1 << 7 | nalType;

    /* frame->data[0] = 49 << 1; */
    /* frame->data[1] = 1; /1* TID *1/ */
    /* Set the S bit with NAL type */
    /* frame->data[2] = 1 << 7 | nalType; */

    data_pos   = 2;
    data_left -= 2;

    /* Send full payload data packets */
    while (data_left + 3 > MAX_PAYLOAD) {
        memcpy(&frame->data[3], &data[data_pos], MAX_PAYLOAD - 3);

        /* if ((ret = RTPGeneric::pushGenericFrame(conn, frame))) */
        /*     goto end; */

        if ((ret = kvz_rtp::sender::write_generic_frame(conn, frame)) != RTP_OK)
            goto end;

        data_pos  += (MAX_PAYLOAD - 3);
        data_left -= (MAX_PAYLOAD - 3);

        /* Clear extra bits */
        /* frame->data[2] = nalType; */
        hevc_fu_hdr[0] = nalType;
    }

    /* Signal end and send the rest of the data */
    hevc_fu_hdr[0] |= 1 << 6;
    /* frame->data[2] |= 1 << 6; */
    memcpy(&frame->data[3], &data[data_pos], data_left);

    ret = kvz_rtp::generic::push_generic_frame(conn, frame->data, data_left + 3, timestamp);

end:
    kvz_rtp::frame::dealloc_frame(frame);
    return ret;
}

rtp_error_t kvz_rtp::hevc::push_hevc_frame(kvz_rtp::connection *conn, uint8_t *data, size_t data_len, uint32_t timestamp)
{
    uint8_t start_len;
    int32_t prev_offset = 0;
    int offset = __internal_get_next_frame_start(data, 0, data_len, start_len);
    prev_offset = offset;

    while (offset != -1) {
        offset = __internal_get_next_frame_start(data, offset, data_len, start_len);

        if (offset > 4 && offset != -1) {
            if (__internal_push_hevc_frame(conn, &data[prev_offset], offset - prev_offset - start_len, timestamp) == -1)
                return RTP_GENERIC_ERROR;

            prev_offset = offset;
        }
    }

    if (prev_offset == -1)
        prev_offset = 0;

    return __internal_push_hevc_frame(conn, &data[prev_offset], data_len - prev_offset, timestamp);
}
