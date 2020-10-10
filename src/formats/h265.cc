#ifdef _WIN32
#else
#include <sys/socket.h>
#endif

#include <cstdint>
#include <cstring>
#include <iostream>
#include <unordered_map>
#include <queue>

#include "debug.hh"
#include "queue.hh"

#include "formats/h265.hh"

void uvg_rtp::formats::h265::clear_aggregation_info()
{
    aggr_pkt_info_.nalus.clear();
    aggr_pkt_info_.aggr_pkt.clear();
}

rtp_error_t uvg_rtp::formats::h265::make_aggregation_pkt()
{
    rtp_error_t ret;

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

    /* create header for the packet and craft the aggregation packet
     * according to the format defined in RFC 7798 */
    aggr_pkt_info_.nal_header[0] = H265_PKT_AGGR << 1;
    aggr_pkt_info_.nal_header[1] = 1;

    aggr_pkt_info_.aggr_pkt.push_back(
        std::make_pair(
            uvg_rtp::frame::HEADER_SIZE_H265_NAL,
            aggr_pkt_info_.nal_header
        )
    );

    for (size_t i = 0; i < aggr_pkt_info_.nalus.size(); ++i) {
        auto pkt_size                   = aggr_pkt_info_.nalus[i].first;
        aggr_pkt_info_.nalus[i].first = htons(aggr_pkt_info_.nalus[i].first);

        aggr_pkt_info_.aggr_pkt.push_back(
            std::make_pair(
                sizeof(uint16_t),
                (uint8_t *)&aggr_pkt_info_.nalus[i].first
            )
        );

        aggr_pkt_info_.aggr_pkt.push_back(
            std::make_pair(
                pkt_size,
                aggr_pkt_info_.nalus[i].second
            )
        );
    }

    if ((ret = fqueue_->enqueue_message(aggr_pkt_info_.aggr_pkt)) != RTP_OK) {
        LOG_ERROR("Failed to enqueue buffers of an aggregation packet!");
        return ret;
    }

    return ret;
}

rtp_error_t uvg_rtp::formats::h265::push_nal_unit(uint8_t *data, size_t data_len, bool more)
{
    if (data_len <= 3)
        return RTP_INVALID_VALUE;

    uint8_t nal_type    = (data[0] >> 1) & 0x3f;
    rtp_error_t ret     = RTP_OK;
    size_t data_left    = data_len;
    size_t data_pos     = 0;
    size_t payload_size = rtp_ctx_->get_payload_size();

    if (data_len - 3 <= payload_size) {
        /* If there is more data coming in (possibly another small packet)
         * create entry to "aggr_pkt_info_" to construct an aggregation packet */
        if (more) {
            aggr_pkt_info_.nalus.push_back(std::make_pair(data_len, data));
            return RTP_NOT_READY;
        } else {
            if (aggr_pkt_info_.nalus.empty()) {
                if ((ret = fqueue_->enqueue_message(data, data_len)) != RTP_OK) {
                    LOG_ERROR("Failed to enqueue Single NAL Unit packet!");
                    return ret;
                }
                return fqueue_->flush_queue();
            } else {
                (void)make_aggregation_pkt();
                ret = fqueue_->flush_queue();
                clear_aggregation_info();
                return ret;
            }
        }
    } else {
        /* If smaller NALUs were queued before this NALU,
         * send them in an aggregation packet before proceeding with fragmentation */
        (void)make_aggregation_pkt();
    }

    /* The payload is larger than MTU (1500 bytes) so we must split it into smaller RTP frames
     * Because we don't if the SCD is enabled and thus cannot make any assumptions about the life time
     * of current stack, we need to store NAL and FU headers to the frame queue transaction.
     *
     * This can be done by asking a handle to current transaction's buffer vectors.
     *
     * During Connection initialization, the frame queue was given HEVC as the payload format so the
     * transaction also contains our media-specifi headers [get_media_headers()]. */
    auto buffers = fqueue_->get_buffer_vector();
    auto headers = (uvg_rtp::formats::h265_headers *)fqueue_->get_media_headers();

    headers->nal_header[0] = H265_PKT_FRAG << 1; /* fragmentation unit */
    headers->nal_header[1] = 1;                  /* temporal id */

    headers->fu_headers[0] = (uint8_t)((1 << 7) | nal_type);
    headers->fu_headers[1] = nal_type;
    headers->fu_headers[2] = (uint8_t)((1 << 6) | nal_type);

    buffers.push_back(std::make_pair(sizeof(headers->nal_header), headers->nal_header));
    buffers.push_back(std::make_pair(sizeof(uint8_t),             &headers->fu_headers[0]));
    buffers.push_back(std::make_pair(payload_size,                nullptr));

    data_pos   = uvg_rtp::frame::HEADER_SIZE_H265_NAL;
    data_left -= uvg_rtp::frame::HEADER_SIZE_H265_NAL;

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
        LOG_ERROR("Failed to send HEVC frame!");
        clear_aggregation_info();
        fqueue_->deinit_transaction();
        return ret;
    }

    if (more)
        return RTP_NOT_READY;

    clear_aggregation_info();
    return fqueue_->flush_queue();
}

uvg_rtp::formats::h265::h265(uvg_rtp::socket *socket, uvg_rtp::rtp *rtp, int flags):
    h26x(socket, rtp, flags), finfo_{}
{
}

uvg_rtp::formats::h265::~h265()
{
}

uvg_rtp::formats::h265_frame_info_t *uvg_rtp::formats::h265::get_h265_frame_info()
{
    return &finfo_;
}

rtp_error_t uvg_rtp::formats::h265::frame_getter(void *arg, uvg_rtp::frame::rtp_frame **frame)
{
    auto finfo = (uvg_rtp::formats::h265_frame_info_t *)arg;

    if (finfo->queued.size()) {
        *frame = finfo->queued.front();
        finfo->queued.pop_front();
        return RTP_PKT_READY;
    }

    return RTP_NOT_FOUND;
}
