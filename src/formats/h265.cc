#include "h265.hh"

#include "../srtp/srtcp.hh"
#include "../rtp.hh"
#include "../frame_queue.hh"

#include "debug.hh"

#include <cstdint>
#include <cstring>
#include <iostream>
#include <unordered_map>
#include <queue>
#include <map>
#include <unordered_set>

#ifndef _WIN32
#include <sys/socket.h>
#endif



uvgrtp::formats::h265::h265(std::shared_ptr<uvgrtp::socket> socket, 
    std::shared_ptr<uvgrtp::rtp> rtp, int rce_flags) :
    h26x(socket, rtp, rce_flags)
{}

uvgrtp::formats::h265::~h265()
{}

uint8_t uvgrtp::formats::h265::get_payload_header_size() const
{
    return HEADER_SIZE_H265_PAYLOAD;
}

uint8_t uvgrtp::formats::h265::get_nal_header_size() const
{
    return HEADER_SIZE_H265_NAL;
}

uint8_t uvgrtp::formats::h265::get_fu_header_size() const
{
    return HEADER_SIZE_H265_FU;
}

uint8_t uvgrtp::formats::h265::get_start_code_range() const
{
    return 4;
}

uvgrtp::formats::NAL_TYPE uvgrtp::formats::h265::get_nal_type(uvgrtp::frame::rtp_frame* frame) const
{
    // see https://datatracker.ietf.org/doc/html/rfc7798#section-4.4.3
    switch (frame->payload[2] & 0x3f) {
    case H265_IDR_W_RADL: return uvgrtp::formats::NAL_TYPE::NT_INTRA;
    case H265_TRAIL_R:    return uvgrtp::formats::NAL_TYPE::NT_INTER;
    default: break;
    }

    return uvgrtp::formats::NAL_TYPE::NT_OTHER;
}

uint8_t uvgrtp::formats::h265::get_nal_type(uint8_t* data) const
{
    // see https://datatracker.ietf.org/doc/html/rfc7798#section-1.1.4
    return (data[0] >> 1) & 0x3f;
}

uvgrtp::formats::FRAG_TYPE uvgrtp::formats::h265::get_fragment_type(uvgrtp::frame::rtp_frame* frame) const
{
    bool first_frag = frame->payload[2] & 0x80; // S bit
    bool last_frag = frame->payload[2] & 0x40;  // E bit

    if ((frame->payload[0] >> 1) == uvgrtp::formats::H265_PKT_AGGR)
        return uvgrtp::formats::FRAG_TYPE::FT_AGGR;

    if ((frame->payload[0] >> 1) != uvgrtp::formats::H265_PKT_FRAG)
        return uvgrtp::formats::FRAG_TYPE::FT_NOT_FRAG; // Single NAL unit

    if (first_frag && last_frag)
        return uvgrtp::formats::FRAG_TYPE::FT_INVALID;

    if (first_frag)
        return uvgrtp::formats::FRAG_TYPE::FT_START;

    if (last_frag)
        return uvgrtp::formats::FRAG_TYPE::FT_END;

    return uvgrtp::formats::FRAG_TYPE::FT_MIDDLE;
}

void uvgrtp::formats::h265::get_nal_header_from_fu_headers(size_t fptr, uint8_t* frame_payload, uint8_t* complete_payload)
{
    uint8_t payload_header[2] = {
        (uint8_t)((frame_payload[0] & 0x81) | ((frame_payload[2] & 0x3f) << 1)),
        (uint8_t)frame_payload[1]
    };

    std::memcpy(&complete_payload[fptr], payload_header, get_payload_header_size());
}

void uvgrtp::formats::h265::clear_aggregation_info()
{
    aggr_pkt_info_.nalus.clear();
    aggr_pkt_info_.aggr_pkt.clear();
}

rtp_error_t uvgrtp::formats::h265::finalize_aggregation_pkt()
{
    rtp_error_t ret = RTP_OK;

    if (aggr_pkt_info_.nalus.size() <= 1)
        return RTP_INVALID_VALUE;

    /* create header for the packet and craft the aggregation packet
     * according to the format defined in RFC 7798 */
    aggr_pkt_info_.payload_header[0] = H265_PKT_AGGR << 1;
    aggr_pkt_info_.payload_header[1] = 1;

    aggr_pkt_info_.aggr_pkt.push_back(
        std::make_pair(HEADER_SIZE_H265_PAYLOAD, aggr_pkt_info_.payload_header)
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
            UVG_LOG_ERROR("NALU too large");
        }
    }

    if ((ret = fqueue_->enqueue_message(aggr_pkt_info_.aggr_pkt)) != RTP_OK) {
        UVG_LOG_ERROR("Failed to enqueue buffers of an aggregation packet!");
        return ret;
    }

    return ret;
}

rtp_error_t uvgrtp::formats::h265::add_aggregate_packet(uint8_t* data, size_t data_len)
{
    /* If there is more data coming in (possibly another small packet)
     * create entry to "aggr_pkt_info_" to construct an aggregation packet */
    aggr_pkt_info_.nalus.push_back(std::make_pair(data_len, data));
    return RTP_OK;
}

rtp_error_t uvgrtp::formats::h265::fu_division(uint8_t* data, size_t data_len, size_t payload_size)
{
    auto headers = (uvgrtp::formats::h265_headers*)fqueue_->get_media_headers();
    
    headers->payload_header[0] = H265_PKT_FRAG << 1; /* fragmentation unit */
    headers->payload_header[1] = 1;                  /* temporal id */

    initialize_fu_headers(get_nal_type(data), headers->fu_headers);

    uvgrtp::buf_vec* buffers = fqueue_->get_buffer_vector();

    // the default structure of one fragment
    buffers->push_back(std::make_pair(sizeof(headers->payload_header), headers->payload_header));
    buffers->push_back(std::make_pair(sizeof(uint8_t), &headers->fu_headers[0])); // first fragment
    buffers->push_back(std::make_pair(payload_size, nullptr));

    return divide_frame_to_fus(data, data_len, payload_size, *buffers, headers->fu_headers);
}
