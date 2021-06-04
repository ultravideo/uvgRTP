#include "formats/h264.hh"

#include "queue.hh"
#include "rtp.hh"
#include "debug.hh"

#include <cstdint>
#include <cstring>
#include <iostream>
#include <unordered_map>
#include <queue>

#ifndef _WIN32
#include <sys/socket.h>
#endif


void uvgrtp::formats::h264::clear_aggregation_info()
{
    aggr_pkt_info_.nalus.clear();
    aggr_pkt_info_.aggr_pkt.clear();
}

rtp_error_t uvgrtp::formats::h264::make_aggregation_pkt()
{
    rtp_error_t ret;
    uint8_t nri = 0;

    if (aggr_pkt_info_.nalus.empty())
        return RTP_INVALID_VALUE;

    /* Only one buffer in the vector -> no need to create an aggregation packet */
    if (aggr_pkt_info_.nalus.size() == 1) {
        if ((ret = fqueue_->enqueue_message(aggr_pkt_info_.nalus)) != RTP_OK) {
            LOG_ERROR("Failed to enqueue Single NAL Unit packet!");
            return ret;
        }

        return fqueue_->flush_queue();
    }

    /* find maximum NRI from given NALUs,
     * it is going to be the NRI value of theSTAP-A header */
    for (auto& nalu : aggr_pkt_info_.nalus) {
        if (((nalu.second[0] >> 5) & 0x3) > nri)
            nri = (nalu.second[0] >> 5) & 0x3;
    }

    /* create header for the packet and craft the aggregation packet
     * according to the format defined in RFC 6184 */
    aggr_pkt_info_.fu_indicator[0] = (0 << 7) | ((nri & 0x3) << 5) | H264_PKT_AGGR;

    aggr_pkt_info_.aggr_pkt.push_back(
        std::make_pair(
            uvgrtp::frame::HEADER_SIZE_H264_FU,
            aggr_pkt_info_.fu_indicator
        )
    );

    for (auto& nalu: aggr_pkt_info_.nalus) {

        if (nalu.first <= UINT16_MAX)
        {
            auto pkt_size = nalu.first;
            nalu.first = htons((u_short)nalu.first);

            aggr_pkt_info_.aggr_pkt.push_back(std::make_pair(sizeof(uint16_t), (uint8_t*)&nalu.first));
            aggr_pkt_info_.aggr_pkt.push_back(std::make_pair(pkt_size, nalu.second));
        }
        else
        {
            LOG_ERROR("NAL unit is too large");
        }
    }

    if ((ret = fqueue_->enqueue_message(aggr_pkt_info_.aggr_pkt)) != RTP_OK) {
        LOG_ERROR("Failed to enqueue NALUs of an aggregation packet!");
    }

    return ret;
}

rtp_error_t uvgrtp::formats::h264::push_nal_unit(uint8_t *data, size_t data_len, bool more)
{
    if (data_len < 2)
        return RTP_INVALID_VALUE;

    uint8_t nal_type    = data[0] & 0x1f;
    rtp_error_t ret     = RTP_OK;
    size_t data_left    = data_len;
    size_t data_pos     = 0;
    size_t payload_size = rtp_ctx_->get_payload_size();

    /* send all packets smaller than MTU as single NAL unit packets */
    if ((size_t)(data_len - 3) <= payload_size) {
        /* If there is more data coming in (possibly another small packet)
         * create entry to "aggr_pkt_info_" to construct an aggregation packet */
        /* if (more) { */
        /*     aggr_pkt_info_.nalus.push_back(std::make_pair(data_len, data)); */
        /*     return RTP_NOT_READY; */
        /* } else { */
        /*     if (aggr_pkt_info_.nalus.empty()) { */
                if ((ret = fqueue_->enqueue_message(data, data_len)) != RTP_OK) {
                    LOG_ERROR("Failed to enqueue Single NAL Unit packet!");
                    return ret;
                }

                if (more)
                    return RTP_NOT_READY;
                return fqueue_->flush_queue();
            /* } else { */
            /*     (void)make_aggregation_pkt(); */
            /*     ret = fqueue_->flush_queue(); */
            /*     clear_aggregation_info(); */
            /*     return ret; */
            /* } */
        /* } */
    } else {
        /* If smaller NALUs were queued before this NALU,
         * send them in an aggregation packet before proceeding with fragmentation */
        /* (void)make_aggregation_pkt(); */
    }

    /* The payload is larger than MTU (1500 bytes) so we must split it into smaller RTP frames
     * Because we don't if the SCD is enabled and thus cannot make any assumptions about the life time
     * of current stack, we need to store NAL and FU headers to the frame queue transaction.
     *
     * This can be done by asking a handle to current transaction's buffer vectors.
     *
     * During Connection initialization, the frame queue was given AVC as the payload format so the
     * transaction also contains our media-specific headers */
    auto buffers = fqueue_->get_buffer_vector();
    auto headers = (uvgrtp::formats::h264_headers *)fqueue_->get_media_headers();

    headers->fu_indicator[0] = (data[0] & 0xe0) | H264_PKT_FRAG;

    headers->fu_headers[0] = (uint8_t)((1 << 7) | nal_type);
    headers->fu_headers[1] = nal_type;
    headers->fu_headers[2] = (uint8_t)((1 << 6) | nal_type);

    buffers.push_back(std::make_pair(sizeof(headers->fu_indicator), headers->fu_indicator));
    buffers.push_back(std::make_pair(sizeof(uint8_t),               &headers->fu_headers[0]));
    buffers.push_back(std::make_pair(payload_size,                  nullptr));

    data_pos   = uvgrtp::frame::HEADER_SIZE_H264_NAL;
    data_left -= uvgrtp::frame::HEADER_SIZE_H264_NAL;

    while (data_left > payload_size) {
        buffers.at(2).first  = payload_size;
        buffers.at(2).second = &data[data_pos];

        if ((ret = fqueue_->enqueue_message(buffers)) != RTP_OK) {
            LOG_ERROR("Queueing the message failed!");
            clear_aggregation_info();
            fqueue_->deinit_transaction();
            return ret;
        }

        data_pos  += payload_size;
        data_left -= payload_size;

        /* from now on, use the FU header meant for middle fragments */
        buffers.at(1).second = &headers->fu_headers[1];
    }

    /* use the FU header meant for the last fragment */
    buffers.at(1).second = &headers->fu_headers[2];

    buffers.at(2).first  = data_left;
    buffers.at(2).second = &data[data_pos];

    if ((ret = fqueue_->enqueue_message(buffers)) != RTP_OK) {
        LOG_ERROR("Failed to send AVC frame!");
        clear_aggregation_info();
        fqueue_->deinit_transaction();
        return ret;
    }

    if (more)
        return RTP_NOT_READY;

    clear_aggregation_info();
    return fqueue_->flush_queue();
}

uvgrtp::formats::h264::h264(uvgrtp::socket *socket, uvgrtp::rtp *rtp, int flags):
    h26x(socket, rtp, flags)
{
}

uvgrtp::formats::h264::~h264()
{
}

uvgrtp::formats::h264_frame_info_t *uvgrtp::formats::h264::get_h264_frame_info()
{
    return &finfo_;
}

rtp_error_t uvgrtp::formats::h264::frame_getter(void *arg, uvgrtp::frame::rtp_frame **frame)
{
    auto finfo = (uvgrtp::formats::h264_frame_info_t *)arg;

    if (finfo->queued.size()) {
        *frame = finfo->queued.front();
        finfo->queued.pop_front();
        return RTP_PKT_READY;
    }

    return RTP_NOT_FOUND;
}
