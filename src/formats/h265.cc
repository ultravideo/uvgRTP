#include "h265.hh"

#include "../srtp/srtcp.hh"
#include "../rtp.hh"
#include "../queue.hh"
#include "debug.hh"

#include <cstdint>
#include <cstring>
#include <iostream>
#include <unordered_map>
#include <queue>

#ifndef _WIN32
#include <sys/socket.h>
#endif


uvgrtp::formats::h265::h265(uvgrtp::socket* socket, uvgrtp::rtp* rtp, int flags) :
    h26x(socket, rtp, flags), finfo_{}
{
    finfo_.rtp_ctx = rtp;
}

uvgrtp::formats::h265::~h265()
{}

void uvgrtp::formats::h265::clear_aggregation_info()
{
    aggr_pkt_info_.nalus.clear();
    aggr_pkt_info_.aggr_pkt.clear();
}

rtp_error_t uvgrtp::formats::h265::make_aggregation_pkt()
{
    rtp_error_t ret;

    if (aggr_pkt_info_.nalus.empty())
        return RTP_INVALID_VALUE;

    /* Only one buffer in the vector -> no need to create an aggregation packet */
    if (aggr_pkt_info_.nalus.size() == 1) {
        if ((ret = fqueue_->enqueue_message(aggr_pkt_info_.nalus)) != RTP_OK) {
            LOG_ERROR("Failed to enqueue Single h265 NAL Unit packet!");
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
            uvgrtp::frame::HEADER_SIZE_H265_NAL,
            aggr_pkt_info_.nal_header
        )
    );

    for (size_t i = 0; i < aggr_pkt_info_.nalus.size(); ++i) {

        if (aggr_pkt_info_.nalus[i].first < UINT16_MAX)
        {
            auto pkt_size = aggr_pkt_info_.nalus[i].first;
            aggr_pkt_info_.nalus[i].first = htons((u_short)aggr_pkt_info_.nalus[i].first);

            aggr_pkt_info_.aggr_pkt.push_back(
                std::make_pair(
                    sizeof(uint16_t),
                    (uint8_t*)&aggr_pkt_info_.nalus[i].first
                )
            );

            aggr_pkt_info_.aggr_pkt.push_back(
                std::make_pair(
                    pkt_size,
                    aggr_pkt_info_.nalus[i].second
                )
            );
        } else {
            LOG_ERROR("NALU too large");
        }
    }

    if ((ret = fqueue_->enqueue_message(aggr_pkt_info_.aggr_pkt)) != RTP_OK) {
        LOG_ERROR("Failed to enqueue buffers of an aggregation packet!");
        return ret;
    }

    return ret;
}

uint8_t uvgrtp::formats::h265::get_nal_type(uint8_t* data)
{
    return (data[0] >> 1) & 0x3f;
}

uvgrtp::formats::h265_frame_info_t *uvgrtp::formats::h265::get_h265_frame_info()
{
    return &finfo_;
}

rtp_error_t uvgrtp::formats::h265::frame_getter(void *arg, uvgrtp::frame::rtp_frame **frame)
{
    auto finfo = (uvgrtp::formats::h265_frame_info_t *)arg;

    if (finfo->queued.size()) {
        *frame = finfo->queued.front();
        finfo->queued.pop_front();
        return RTP_PKT_READY;
    }

    return RTP_NOT_FOUND;
}

rtp_error_t uvgrtp::formats::h265::handle_small_packet(uint8_t* data, size_t data_len, bool more)
{
    /* If there is more data coming in (possibly another small packet)
 * create entry to "aggr_pkt_info_" to construct an aggregation packet */
    if (more) {
        aggr_pkt_info_.nalus.push_back(std::make_pair(data_len, data));
        return RTP_NOT_READY;
    }
    else {
        rtp_error_t ret = RTP_OK;
        if (aggr_pkt_info_.nalus.empty()) {
            if ((ret = fqueue_->enqueue_message(data, data_len)) != RTP_OK) {
                LOG_ERROR("Failed to enqueue Single h265 NAL Unit packet! Size: %lu", data_len);
                return ret;
            }
        }
        else {
            (void)make_aggregation_pkt();
            ret = fqueue_->flush_queue();
            clear_aggregation_info();
            return ret;
        }
    }
}

rtp_error_t uvgrtp::formats::h265::construct_format_header_divide_fus(uint8_t* data, size_t& data_left,
    size_t& data_pos, size_t payload_size, uvgrtp::buf_vec& buffers)
{
    auto headers = (uvgrtp::formats::h265_headers*)fqueue_->get_media_headers();

    headers->nal_header[0] = H265_PKT_FRAG << 1; /* fragmentation unit */
    headers->nal_header[1] = 1;                  /* temporal id */

    initialize_fu_headers(get_nal_type(data), headers->fu_headers);

    buffers.push_back(std::make_pair(sizeof(headers->nal_header), headers->nal_header));
    buffers.push_back(std::make_pair(sizeof(uint8_t), &headers->fu_headers[0]));
    buffers.push_back(std::make_pair(payload_size, nullptr));

    data_pos = uvgrtp::frame::HEADER_SIZE_H265_NAL;
    data_left -= uvgrtp::frame::HEADER_SIZE_H265_NAL;

    return divide_frame_to_fus(data, data_left, data_pos, payload_size, buffers, headers->fu_headers);
}
