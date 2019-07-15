#include <iostream>
#include <cstdint>
#include <cstring>

#include "conn.hh"
#include "debug.hh"
#include "rtp_hevc.hh"
#include "queue.hh"
#include "send.hh"
#include "writer.hh"

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

#ifdef __linux__
    auto fqueue = dynamic_cast<kvz_rtp::writer *>(conn)->get_frame_queue();

    /* all fragment units share the same RTP and HEVC NAL headers
     * but because there's three different types of FU headers (and because the state 
     * of each buffer must last between calls) we must allocate space for three FU headers */
    std::vector<std::pair<size_t, uint8_t *>> buffers;

    uint8_t header[
        kvz_rtp::frame::HEADER_SIZE_RTP +
        kvz_rtp::frame::HEADER_SIZE_HEVC_NAL ] = { 0 };

    /* create common RTP and NAL headers */
    conn->fill_rtp_header(header, timestamp);

    header[kvz_rtp::frame::HEADER_SIZE_RTP + 0]  = 49 << 1; /* fragmentation unit */
    header[kvz_rtp::frame::HEADER_SIZE_RTP + 1]  = 1;       /* TID */

    /* one for first frag, one for all the middle frags and one for the last frag */
    uint8_t fu_headers[3 * kvz_rtp::frame::HEADER_SIZE_HEVC_FU] = {
        (uint8_t)((1 << 7) | nalType),
        nalType,
        (uint8_t)((1 << 6) | nalType)
    };

    buffers.push_back(std::make_pair(sizeof(header),  header));
    buffers.push_back(std::make_pair(sizeof(uint8_t), &fu_headers[0]));
    buffers.push_back(std::make_pair(MAX_PAYLOAD,     nullptr));

    data_pos   = kvz_rtp::frame::HEADER_SIZE_HEVC_NAL;
    data_left -= kvz_rtp::frame::HEADER_SIZE_HEVC_NAL;

    while (data_left > MAX_PAYLOAD) {
        buffers.at(2).first  = MAX_PAYLOAD;
        buffers.at(2).second = &data[data_pos];

        if ((ret = fqueue.enqueue_message(conn, buffers)) != RTP_OK)
            return ret;

        data_pos  += MAX_PAYLOAD;
        data_left -= MAX_PAYLOAD;

        /* from now on, use the FU header meant for middle frags */
        buffers.at(1).second = &fu_headers[1];
    }

    /* use the FU header meant for last fragment */
    buffers.at(1).second = &fu_headers[2];

    buffers.at(2).first  = data_left;
    buffers.at(2).second = &data[data_pos];

    if ((ret = fqueue.enqueue_message(conn, buffers)) != RTP_OK) {
        LOG_ERROR("Failed to send HEVC frame!");
        fqueue.empty_queue();
        return ret;
    }

    return fqueue.flush_queue(conn);
#else
    const size_t HEADER_SIZE =
        kvz_rtp::frame::HEADER_SIZE_RTP +
        kvz_rtp::frame::HEADER_SIZE_HEVC_NAL +
        kvz_rtp::frame::HEADER_SIZE_HEVC_FU;

    uint8_t buffer[HEADER_SIZE + MAX_PAYLOAD];

    conn->fill_rtp_header(buffer, timestamp);

    buffer[kvz_rtp::frame::HEADER_SIZE_RTP + 0]  = 49 << 1;            /* fragmentation unit */
    buffer[kvz_rtp::frame::HEADER_SIZE_RTP + 1]  = 1;                  /* TID */
    buffer[kvz_rtp::frame::HEADER_SIZE_RTP +
           kvz_rtp::frame::HEADER_SIZE_HEVC_NAL] = (1 << 7) | nalType; /* Start bit + NAL type */

    data_pos   = kvz_rtp::frame::HEADER_SIZE_HEVC_NAL;
    data_left -= kvz_rtp::frame::HEADER_SIZE_HEVC_NAL;

    while (data_left > MAX_PAYLOAD) {
        memcpy(&buffer[HEADER_SIZE], &data[data_pos], MAX_PAYLOAD);

        if ((ret = kvz_rtp::sender::write_payload(conn, buffer, sizeof(buffer))) != RTP_OK)
            return RTP_GENERIC_ERROR;

        data_pos  += MAX_PAYLOAD;
        data_left -= MAX_PAYLOAD;

        /* Clear extra bits */
        buffer[kvz_rtp::frame::HEADER_SIZE_RTP +
                   kvz_rtp::frame::HEADER_SIZE_HEVC_NAL] = nalType;
    }

    buffer[kvz_rtp::frame::HEADER_SIZE_RTP +
               kvz_rtp::frame::HEADER_SIZE_HEVC_NAL] |= (1 << 6); /* set E bit to signal end of data */

    memcpy(&buffer[HEADER_SIZE], &data[data_pos], data_left);

    ret = kvz_rtp::sender::write_payload(conn, buffer, HEADER_SIZE + data_left);
#endif

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

    frame->payload_len -= (kvz_rtp::frame::HEADER_SIZE_HEVC_NAL +
                           kvz_rtp::frame::HEADER_SIZE_HEVC_FU);
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

    ret = kvz_rtp::frame::alloc_rtp_frame(fu.first + 2, kvz_rtp::frame::FRAME_TYPE_GENERIC);

    /* copy the RTP header of the first fragmentation unit and use it for the full frame */
    memcpy(frame->data, fu.second.at(0)->data, kvz_rtp::frame::HEADER_SIZE_RTP);

    {
        kvz_rtp::frame::rtp_frame *f = fu.second.at(0);

        NALHeader[0] = (f->payload[0] & 0x81) | ((frame->payload[2] & 0x3f) << 1);
        NALHeader[1] = f->payload[1];

        memcpy(ret->payload, NALHeader, sizeof(NALHeader));

        size_t ptr    = sizeof(NALHeader);
        size_t offset = kvz_rtp::frame::HEADER_SIZE_HEVC_NAL +
                        kvz_rtp::frame::HEADER_SIZE_HEVC_FU;

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
