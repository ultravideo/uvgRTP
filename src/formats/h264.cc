#include "h264.hh"

#include "../frame_queue.hh"
#include "../rtp.hh"

#include "debug.hh"

#include <cstdint>
#include <cstring>
#include <iostream>
#include <unordered_map>
#include <queue>
#include <map>

#ifndef _WIN32
#include <sys/socket.h>
#endif

uvgrtp::formats::h264::h264(std::shared_ptr<uvgrtp::socket> socket, std::shared_ptr<uvgrtp::rtp> rtp, int rce_flags) :
    h26x(socket, rtp, rce_flags)
{
}

uvgrtp::formats::h264::~h264()
{
}

uint8_t uvgrtp::formats::h264::get_payload_header_size() const
{
    return HEADER_SIZE_H264_INDICATOR;
}

uint8_t uvgrtp::formats::h264::get_nal_header_size() const
{
    return HEADER_SIZE_H264_NAL;
}

uint8_t uvgrtp::formats::h264::get_fu_header_size() const
{
    return HEADER_SIZE_H264_FU;
}

uint8_t uvgrtp::formats::h264::get_start_code_range() const
{
    return 1; // H264 can have three byte start codes and therefore we must scan one byte at a time
}

uvgrtp::formats::FRAG_TYPE uvgrtp::formats::h264::get_fragment_type(uvgrtp::frame::rtp_frame* frame) const
{
    bool first_frag = frame->payload[1] & 0x80;
    bool last_frag = frame->payload[1] & 0x40;

    if ((frame->payload[0] & 0x1f) == uvgrtp::formats::H264_STAP_A)
        return uvgrtp::formats::FRAG_TYPE::FT_AGGR;     // STAP-A packet, RFC 6184 5.7.1

    if ((frame->payload[0] & 0x1f) == uvgrtp::formats::H264_STAP_B)
        return uvgrtp::formats::FRAG_TYPE::FT_STAP_B; // STAP-B packet, RFC 6184 5.7.1

    if ((frame->payload[0] & 0x1f) != uvgrtp::formats::H264_PKT_FRAG)
        return uvgrtp::formats::FRAG_TYPE::FT_NOT_FRAG; // Single NAL unit

    if (first_frag && last_frag)
        return uvgrtp::formats::FRAG_TYPE::FT_INVALID;

    if (first_frag)
        return uvgrtp::formats::FRAG_TYPE::FT_START;

    if (last_frag)
        return uvgrtp::formats::FRAG_TYPE::FT_END;

    return uvgrtp::formats::FRAG_TYPE::FT_MIDDLE;
}

uvgrtp::formats::NAL_TYPE uvgrtp::formats::h264::get_nal_type(uvgrtp::frame::rtp_frame* frame) const
{
    // see https://datatracker.ietf.org/doc/html/rfc6184#section-5.8

    switch (frame->payload[1] & 0x1f) {
    case H264_IDR: return uvgrtp::formats::NAL_TYPE::NT_INTRA;
    case H264_NON_IDR:  return uvgrtp::formats::NAL_TYPE::NT_INTER;
    default: break;
    }

    return uvgrtp::formats::NAL_TYPE::NT_OTHER;
}

uint8_t uvgrtp::formats::h264::get_nal_type(uint8_t* data) const
{
    // see https://datatracker.ietf.org/doc/html/rfc6184#section-5.3
    return data[0] & 0x1f;
}

void uvgrtp::formats::h264::clear_aggregation_info()
{
    aggr_pkt_info_.nalus.clear();
    aggr_pkt_info_.aggr_pkt.clear();
}

rtp_error_t uvgrtp::formats::h264::add_aggregate_packet(uint8_t* data, size_t data_len)
{
    /* If there is more data coming in (possibly another small packet)
     * create entry to "aggr_pkt_info_" to construct an aggregation packet */
    aggr_pkt_info_.nalus.push_back(std::make_pair(data_len, data));
    return RTP_OK;
}

rtp_error_t uvgrtp::formats::h264::finalize_aggregation_pkt()
{
    rtp_error_t ret = RTP_OK;
    uint8_t nri = 0;

    if (aggr_pkt_info_.nalus.size() <= 1)
        return RTP_INVALID_VALUE;

    /* find maximum NRI from given NALUs,
     * it is going to be the NRI value of theSTAP-A header */
    for (auto& nalu : aggr_pkt_info_.nalus) {
        if (((nalu.second[0] >> 5) & 0x3) > nri)
            nri = (nalu.second[0] >> 5) & 0x3;
    }

    /* create header for the packet and craft the aggregation packet
     * according to the format defined in RFC 6184 */
    aggr_pkt_info_.fu_indicator[0] = (0 << 7) | ((nri & 0x3) << 5) | H264_STAP_A;

    aggr_pkt_info_.aggr_pkt.push_back(
        std::make_pair(HEADER_SIZE_H264_FU, aggr_pkt_info_.fu_indicator)
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
            UVG_LOG_ERROR("NAL unit is too large");
        }
    }

    if ((ret = fqueue_->enqueue_message(aggr_pkt_info_.aggr_pkt)) != RTP_OK) {
        UVG_LOG_ERROR("Failed to enqueue NALUs of an aggregation packet!");
    }

    return ret;
}

rtp_error_t uvgrtp::formats::h264::fu_division(uint8_t* data, size_t data_len, size_t payload_size)
{
    auto headers = (uvgrtp::formats::h264_headers*)fqueue_->get_media_headers();

    headers->fu_indicator[0] = (data[0] & 0xe0) | H264_PKT_FRAG;

    initialize_fu_headers(get_nal_type(data), headers->fu_headers);

    uvgrtp::buf_vec* buffers = fqueue_->get_buffer_vector();
    buffers->push_back(std::make_pair(sizeof(headers->fu_indicator), headers->fu_indicator));
    buffers->push_back(std::make_pair(sizeof(uint8_t), &headers->fu_headers[0]));
    buffers->push_back(std::make_pair(payload_size, nullptr));

    return divide_frame_to_fus(data, data_len, payload_size, *buffers, headers->fu_headers);
}

void uvgrtp::formats::h264::get_nal_header_from_fu_headers(size_t fptr, uint8_t* frame_payload, uint8_t* complete_payload)
{
    complete_payload[fptr] = (frame_payload[0] & 0xe0) | (frame_payload[1] & 0x1f);
}

uvgrtp::frame::rtp_frame* uvgrtp::formats::h264::allocate_rtp_frame_with_startcode(bool add_start_code,
    uvgrtp::frame::rtp_header& header, size_t payload_size_without_startcode, size_t& fptr)
{
    uvgrtp::frame::rtp_frame* complete = uvgrtp::frame::alloc_rtp_frame();

    complete->payload_len = payload_size_without_startcode;

    if (add_start_code) {
        complete->payload_len += 3;
    }

    complete->payload = new uint8_t[complete->payload_len];

    if (add_start_code && complete->payload_len >= 3) {
        complete->payload[0] = 0;
        complete->payload[1] = 0;
        complete->payload[2] = 1;
        fptr += 3;
    }

    complete->header = header; // copy

    return complete;
}

void uvgrtp::formats::h264::prepend_start_code(int rce_flags, uvgrtp::frame::rtp_frame** out)
{
    if (!(rce_flags & RCE_NO_H26X_PREPEND_SC)) {
        uint8_t* pl = new uint8_t[(*out)->payload_len + 3];

        pl[0] = 0;
        pl[1] = 0;
        pl[2] = 1;

        std::memcpy(pl + 3, (*out)->payload, (*out)->payload_len);
        delete[](*out)->payload;

        (*out)->payload = pl;
        (*out)->payload_len += 3;
    }
}
