#include <iostream>
#include <cstdint>
#include <cstring>

#include "conn.hh"
#include "debug.hh"
#include "rtp_hevc.hh"
#include "send.hh"

static int __get_next_frame_start(uint8_t *data, uint32_t offset, uint32_t data_len, uint8_t& start_len)
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

static rtp_error_t __push_hevc_frame(kvz_rtp::connection *conn, uint8_t *data, size_t data_len, uint32_t timestamp)
{
    uint8_t nalType  = (data[0] >> 1) & 0x3F;
    rtp_error_t ret  = RTP_OK;
    size_t data_left = data_len;
    size_t data_pos  = 0;

    if (data_len <= MAX_PAYLOAD) {
        LOG_DEBUG("send unfrag size %zu, type %u", data_len, nalType);
        return kvz_rtp::generic::push_generic_frame(conn, data, data_len, timestamp);
    }

    LOG_DEBUG("send frag size: %zu, type %u", data_len, nalType);

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
    int offset = __get_next_frame_start(data, 0, data_len, start_len);
    prev_offset = offset;

    while (offset != -1) {
        offset = __get_next_frame_start(data, offset, data_len, start_len);

        if (offset > 4 && offset != -1) {
            if (__push_hevc_frame(conn, &data[prev_offset], offset - prev_offset - start_len, timestamp) == -1)
                return RTP_GENERIC_ERROR;

            prev_offset = offset;
        }
    }

    if (prev_offset == -1)
        prev_offset = 0;

    return __push_hevc_frame(conn, &data[prev_offset], data_len - prev_offset, timestamp);
}

kvz_rtp::frame::rtp_frame *kvz_rtp::hevc::process_hevc_frame(
        kvz_rtp::frame::rtp_frame *frame,
        std::pair<size_t, std::vector<kvz_rtp::frame::rtp_frame *>>& fu,
        rtp_error_t& error
)
{
    bool last_frag                 = false;
    bool first_frag                = false;
    uint8_t NALHeader[2]           = { 0 };
    kvz_rtp::frame::rtp_frame *ret = nullptr;

    if (!frame) {
        LOG_ERROR("Invalid value, unable to process frame!");

        error = RTP_INVALID_VALUE;
        return nullptr;
    }

    if ((frame->payload[0] >> 1) != 49) {
        LOG_DEBUG("frame is not fragmented, returning...");
        error = RTP_OK;

        /* we received a non-fragmented HEVC frame but FU buffer is not empty 
         * which means a packet was lost and we must drop this partial frame */
        if (!fu.second.empty()) {
            /* TODO: update rtcp stats for dropped packets */

            LOG_WARN("fragmented frame was not fully received, dropping...");
            ret = frame;
            goto error;
        }

        return frame;
    }

    if (!fu.second.empty()) {
        if (frame->timestamp != fu.second.at(0)->timestamp) {
            LOG_ERROR("Timestamp mismatch for fragmentation units!");

            /* TODO: update rtcp stats for dropped packets */

            /* push the frame to fu vector so deallocation is cleaner */
            fu.second.push_back(frame);
            goto error;
        }
    }

    first_frag = frame->payload[2] & 0x80;
    last_frag  = frame->payload[2] & 0x40;

    if (first_frag && last_frag) {
        LOG_ERROR("Invalid combination of S and E bits");

        /* push the frame to fu vector so deallocation is cleaner */
        fu.second.push_back(frame);

        error = RTP_INVALID_VALUE;
        goto error;
    }

    frame->payload_len -= (HEVC_RTP_HEADER_SIZE + HEVC_FU_HEADER_SIZE);
    fu.second.push_back(frame);
    fu.first += frame->payload_len;

    if (!last_frag) {
        error = RTP_NOT_READY;
        return frame;
    }

    if (fu.second.size() == 1) {
        LOG_ERROR("Received the last FU but FU vector is empty!");
        error = RTP_INVALID_VALUE;
        goto error;
    }

    ret = kvz_rtp::frame::alloc_frame(fu.first + 2, kvz_rtp::frame::FRAME_TYPE_GENERIC);

    /* copy the RTP header of the first fragmentation unit and use it for the full frame */
    memcpy(frame->data, fu.second.at(0)->data, kvz_rtp::frame::HEADER_SIZE_RTP);

    {
        kvz_rtp::frame::rtp_frame *f = fu.second.at(0);

        NALHeader[0] = (f->payload[0] & 0x81) | ((frame->payload[2] & 0x3f) << 1);
        NALHeader[1] = f->payload[1];

        memcpy(ret->payload, NALHeader, sizeof(NALHeader));

        size_t ptr    = sizeof(NALHeader);
        size_t offset = HEVC_RTP_HEADER_SIZE + HEVC_FU_HEADER_SIZE;

        for (auto& i : fu.second) {

            memcpy(&ret->payload[ptr], i->payload + offset, i->payload_len);
            ptr += i->payload_len;
        }
    }

    while (fu.second.size() != 0) {
        auto tmp = fu.second.back();
        fu.second.pop_back();
        kvz_rtp::frame::dealloc_frame(tmp);
    }

    /* reset the total size of fragmentation units */
    fu.first = 0;

    error = RTP_OK;
    return ret;

error:
    while (fu.second.size() != 0) {
        auto tmp = fu.second.back();
        fu.second.pop_back();
        kvz_rtp::frame::dealloc_frame(tmp);
    }

    /* reset the total size of fragmentation units */
    fu.first = 0;

    return ret;
}
