#include "v3c.hh"

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



uvgrtp::formats::v3c::v3c(std::shared_ptr<uvgrtp::socket> socket, std::shared_ptr<uvgrtp::rtp> rtp, int rce_flags) :
    h26x(socket, rtp, rce_flags)
{}

uvgrtp::formats::v3c::~v3c()
{
}

uint8_t uvgrtp::formats::v3c::get_payload_header_size() const
{
    return HEADER_SIZE_V3C_PAYLOAD;
}

uint8_t uvgrtp::formats::v3c::get_nal_header_size() const
{
    return HEADER_SIZE_V3C_NAL;
}

uint8_t uvgrtp::formats::v3c::get_fu_header_size() const
{
    return HEADER_SIZE_V3C_FU;
}

uint8_t uvgrtp::formats::v3c::get_start_code_range() const
{
    return 4;
}

uvgrtp::formats::NAL_TYPE uvgrtp::formats::v3c::get_nal_type(uvgrtp::frame::rtp_frame* frame) const
{
    uint8_t nal_type = frame->payload[2] & 0x3f;
    /*
    if (nal_type == H266_IDR_W_RADL)
        return uvgrtp::formats::NAL_TYPE::NT_INTRA;
    else if (nal_type == H266_TRAIL_NUT)
        return uvgrtp::formats::NAL_TYPE::NT_INTER;
        */
    return uvgrtp::formats::NAL_TYPE::NT_OTHER;
}

uint8_t uvgrtp::formats::v3c::get_nal_type(uint8_t* data) const
{
    //return (data[1] >> 3) & 0x1f;
    return data[0] & 0x10F447;
}

uvgrtp::formats::FRAG_TYPE uvgrtp::formats::v3c::get_fragment_type(uvgrtp::frame::rtp_frame* frame) const
{
    // Same bits as in VVC FU headers
    bool first_frag = frame->payload[2] & 0x80;
    bool last_frag = frame->payload[2] & 0x40;

    if ((frame->payload[1] >> 3) != uvgrtp::formats::V3C_PKT_FRAG)
        return uvgrtp::formats::FRAG_TYPE::FT_NOT_FRAG; // Single NAL unit

    if (first_frag && last_frag)
        return uvgrtp::formats::FRAG_TYPE::FT_INVALID;

    if (first_frag)
        return uvgrtp::formats::FRAG_TYPE::FT_START;

    if (last_frag)
        return uvgrtp::formats::FRAG_TYPE::FT_END;

    return uvgrtp::formats::FRAG_TYPE::FT_MIDDLE;
}

void uvgrtp::formats::v3c::get_nal_header_from_fu_headers(size_t fptr, uint8_t* frame_payload, uint8_t* complete_payload)
{
    /* construct the NAL header from fragment header of current fragment

    V3C                             V3C FU
    +---------------+---------------+---------------+
    |0|1|2|3|4|5|6|7|0|1|2|3|4|5|6|7|0|1|2|3|4|5|6|7|
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |F|    NUT    |    NLI    | TID |S|E|    FUT    |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-----------+

    Erase the original NUT (Frag = 58) and replace it with the FUT. Rest of the header is not changed */
    // 0x81 = 1000 0001
    uint8_t payload_header[2] = {
        (uint8_t)((frame_payload[0] & 0x81) | ((frame_payload[2] & 0x3f) << 1)),
        (uint8_t)(frame_payload[1])
    };
    std::memcpy(&complete_payload[fptr], payload_header, get_payload_header_size());
}

rtp_error_t uvgrtp::formats::v3c::fu_division(uint8_t* data, size_t data_len, size_t payload_size)
{
    auto headers = (uvgrtp::formats::v3c_headers*)fqueue_->get_media_headers();
    // 0x81 = 1000 0001
    headers->payload_header[0] = (V3C_PKT_FRAG << 1) | (data[0] & 0x81);
    headers->payload_header[1] = data[1];

    initialize_fu_headers(get_nal_type(data), headers->fu_headers);

    uvgrtp::buf_vec* buffers = fqueue_->get_buffer_vector();

    buffers->push_back(std::make_pair(sizeof(headers->payload_header), headers->payload_header));
    buffers->push_back(std::make_pair(sizeof(uint8_t), &headers->fu_headers[0]));
    buffers->push_back(std::make_pair(payload_size, nullptr));

    return divide_frame_to_fus(data, data_len, payload_size, *buffers, headers->fu_headers);
}