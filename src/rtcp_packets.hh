#pragma once

#include "uvgrtp/frame.hh"

#include <vector>


namespace uvgrtp
{
    const uint16_t RTCP_HEADER_SIZE = 4;
    const uint16_t SSRC_CSRC_SIZE = 4;
    const uint16_t SENDER_INFO_SIZE = 20;
    const uint16_t REPORT_BLOCK_SIZE = 24;
    const uint16_t APP_NAME_SIZE = 4;

    /* Set the first four or eight bytes of an RTCP packet */
    bool construct_rtcp_header(uint8_t* frame, int& ptr, size_t packet_size,
        uint16_t secondField, uvgrtp::frame::RTCP_FRAME_TYPE frame_type, uint32_t ssrc);

    size_t get_sr_packet_size(int flags, uint16_t reports);
    size_t get_rr_packet_size(int flags, uint16_t reports);
    size_t get_sdes_packet_size(const std::vector<uvgrtp::frame::rtcp_sdes_item>& items);
    size_t get_app_packet_size(size_t payload_len);
    size_t get_bye_packet_size(const std::vector<uint32_t>& ssrcs);

    bool construct_sdes_packet(uint8_t* frame, int& ptr,
        const std::vector<uvgrtp::frame::rtcp_sdes_item>& items);
    bool construct_app_packet(uint8_t* frame, int& ptr,
        const char* name, const uint8_t* payload, size_t payload_len);
    bool construct_bye_packet(uint8_t* frame, int& ptr, const std::vector<uint32_t>& ssrcs);
}