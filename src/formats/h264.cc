#include "h264.hh"

#include "../queue.hh"
#include "../rtp.hh"
#include "debug.hh"

#include <cstdint>
#include <cstring>
#include <iostream>
#include <unordered_map>
#include <queue>

#ifndef _WIN32
#include <sys/socket.h>
#endif

uvgrtp::formats::h264::h264(uvgrtp::socket* socket, uvgrtp::rtp* rtp, int flags) :
    h26x(socket, rtp, flags)
{
}

uvgrtp::formats::h264::~h264()
{
}

void uvgrtp::formats::h264::clear_aggregation_info()
{
    aggr_pkt_info_.nalus.clear();
    aggr_pkt_info_.aggr_pkt.clear();
}

rtp_error_t uvgrtp::formats::h264::make_aggregation_pkt()
{
    rtp_error_t ret = RTP_OK;

    // TODO: This function is not used, but the code exists for some reason. 
    // This return disables the code for now
    return ret;

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


uint8_t uvgrtp::formats::h264::get_nal_type(uint8_t* data)
{
    return data[0] & 0x1f;
}

rtp_error_t uvgrtp::formats::h264::handle_small_packet(uint8_t* data, size_t data_len, bool more)
{
    rtp_error_t ret = RTP_OK;
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

rtp_error_t uvgrtp::formats::h264::construct_format_header_divide_fus(uint8_t* data, size_t& data_left,
    size_t& data_pos, size_t payload_size, uvgrtp::buf_vec& buffers)
{
    auto headers = (uvgrtp::formats::h264_headers*)fqueue_->get_media_headers();

    headers->fu_indicator[0] = (data[0] & 0xe0) | H264_PKT_FRAG;

    initialize_fu_headers(get_nal_type(data), headers->fu_headers);

    buffers.push_back(std::make_pair(sizeof(headers->fu_indicator), headers->fu_indicator));
    buffers.push_back(std::make_pair(sizeof(uint8_t), &headers->fu_headers[0]));
    buffers.push_back(std::make_pair(payload_size, nullptr));

    data_pos = uvgrtp::frame::HEADER_SIZE_H264_NAL;
    data_left -= uvgrtp::frame::HEADER_SIZE_H264_NAL;

    return divide_frame_to_fus(data, data_left, data_pos, payload_size, buffers, headers->fu_headers);
}