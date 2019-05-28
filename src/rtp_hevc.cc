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

static rtp_error_t __internal_push_hevc_frame(kvz_rtp::connection *conn, uint8_t *data, size_t data_len, uint32_t timestamp)
{
    uint32_t data_pos  = 0;
    uint32_t data_left = data_len;
    rtp_error_t ret    = RTP_OK;
    uint8_t nalType    = (data[0] >> 1) & 0x3F;

    if (data_len <= MAX_PAYLOAD) {
        LOG_DEBUG("send unfrag size %u, type %u", data_len, nalType);
        return kvz_rtp::generic::push_generic_frame(conn, data, data_len, timestamp);
    }

    LOG_DEBUG("send frag size: %u, type %u", data_len, nalType);

    uint8_t header[
        kvz_rtp::frame::HEADER_SIZE_RTP      +
        kvz_rtp::frame::HEADER_SIZE_HEVC_RTP +
        kvz_rtp::frame::HEADER_SIZE_HEVC_FU  ] = { 0 };

    conn->fill_rtp_header(header, timestamp);

    header[kvz_rtp::frame::HEADER_SIZE_RTP + 0]  = 49 << 1;            /* fragmentation unit */
    header[kvz_rtp::frame::HEADER_SIZE_RTP + 1]  = 1;                  /* TID */
    header[kvz_rtp::frame::HEADER_SIZE_RTP +
           kvz_rtp::frame::HEADER_SIZE_HEVC_RTP] = (1 << 7) | nalType; /* Start bit + NAL type */

    data_pos   = kvz_rtp::frame::HEADER_SIZE_HEVC_RTP;
    data_left -= kvz_rtp::frame::HEADER_SIZE_HEVC_RTP;

    while (data_left > MAX_PAYLOAD) {
        if ((ret = kvz_rtp::sender::write_frame(conn, header, sizeof(header), &data[data_pos], MAX_PAYLOAD)) != RTP_OK)
            goto end;

        data_pos  += MAX_PAYLOAD;
        data_left -= MAX_PAYLOAD;

        /* Clear extra bits */
        header[kvz_rtp::frame::HEADER_SIZE_RTP +
               kvz_rtp::frame::HEADER_SIZE_HEVC_RTP] = nalType;
    }

    header[kvz_rtp::frame::HEADER_SIZE_RTP +
           kvz_rtp::frame::HEADER_SIZE_HEVC_RTP] |= (1 << 6); /* set E bit to signal end of data */

    ret = kvz_rtp::sender::write_frame(conn, header, sizeof(header), &data[data_pos], data_left);

end:
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
