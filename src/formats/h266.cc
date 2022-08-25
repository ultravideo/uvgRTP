#include "h266.hh"

#include "uvgrtp/frame.hh"

#include "../rtp.hh"
#include "../frame_queue.hh"
#include "debug.hh"

#include <cstdint>
#include <cstring>
#include <iostream>
#include <unordered_map>
#include <unordered_set>
#include <map>
#include <queue>


#ifndef _WIN32
#include <sys/socket.h>
#endif



uvgrtp::formats::h266::h266(std::shared_ptr<uvgrtp::socket> socket, std::shared_ptr<uvgrtp::rtp> rtp, int rce_flags) :
    h26x(socket, rtp, rce_flags)
{}

uvgrtp::formats::h266::~h266()
{
}

uint8_t uvgrtp::formats::h266::get_payload_header_size() const
{
    return HEADER_SIZE_H266_PAYLOAD;
}

uint8_t uvgrtp::formats::h266::get_nal_header_size() const
{
    return HEADER_SIZE_H266_NAL;
}

uint8_t uvgrtp::formats::h266::get_fu_header_size() const
{
    return HEADER_SIZE_H266_FU;
}

uint8_t uvgrtp::formats::h266::get_start_code_range() const
{
    return 4;
}

uvgrtp::formats::NAL_TYPE uvgrtp::formats::h266::get_nal_type(uvgrtp::frame::rtp_frame* frame) const
{
    // see https://datatracker.ietf.org/doc/html/draft-ietf-avtcore-rtp-vvc#section-4.3.3
    uint8_t nal_type = frame->payload[2] & 0x3f;

    if (nal_type == H266_IDR_W_RADL)
        return uvgrtp::formats::NAL_TYPE::NT_INTRA;
    else if (nal_type == H266_TRAIL_NUT)
        return uvgrtp::formats::NAL_TYPE::NT_INTER;

    return uvgrtp::formats::NAL_TYPE::NT_OTHER;
}

uint8_t uvgrtp::formats::h266::get_nal_type(uint8_t* data) const
{
    // see https://datatracker.ietf.org/doc/html/draft-ietf-avtcore-rtp-vvc#section-1.1.4
    return (data[1] >> 3) & 0x1f;
}

uvgrtp::formats::FRAG_TYPE uvgrtp::formats::h266::get_fragment_type(uvgrtp::frame::rtp_frame* frame) const
{
    bool first_frag = frame->payload[2] & 0x80;
    bool last_frag = frame->payload[2] & 0x40;

    if ((frame->payload[1] >> 3) != uvgrtp::formats::H266_PKT_FRAG)
        return uvgrtp::formats::FRAG_TYPE::FT_NOT_FRAG; // Single NAL unit

    if (first_frag && last_frag)
        return uvgrtp::formats::FRAG_TYPE::FT_INVALID;

    if (first_frag)
        return uvgrtp::formats::FRAG_TYPE::FT_START;

    if (last_frag)
        return uvgrtp::formats::FRAG_TYPE::FT_END;

    return uvgrtp::formats::FRAG_TYPE::FT_MIDDLE;
}

void uvgrtp::formats::h266::get_nal_header_from_fu_headers(size_t fptr, uint8_t* frame_payload, uint8_t* complete_payload)
{
    uint8_t payload_header[2] = {
        (uint8_t)((frame_payload[0])),
        (uint8_t)((frame_payload[1]&0x7)|((frame_payload[2] & 0x1f) << 3))
    };

    std::memcpy(&complete_payload[fptr], payload_header, get_payload_header_size());
}


rtp_error_t uvgrtp::formats::h266::fu_division(uint8_t* data, size_t data_len, size_t payload_size)
{
    auto headers = (uvgrtp::formats::h266_headers*)fqueue_->get_media_headers();

    headers->payload_header[0] = data[0];
    headers->payload_header[1] = (H266_PKT_FRAG << 3) | (data[1] & 0x7);

    initialize_fu_headers(get_nal_type(data), headers->fu_headers);

    uvgrtp::buf_vec* buffers = fqueue_->get_buffer_vector();

    buffers->push_back(std::make_pair(sizeof(headers->payload_header), headers->payload_header));
    buffers->push_back(std::make_pair(sizeof(uint8_t), &headers->fu_headers[0]));
    buffers->push_back(std::make_pair(payload_size, nullptr));

    return divide_frame_to_fus(data, data_len, payload_size, *buffers, headers->fu_headers);
}