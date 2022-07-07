#pragma once

#include "uvgrtp/frame.hh"

#include <vector>


const uint16_t RTCP_HEADER_SIZE = 4;
const uint16_t SSRC_CSRC_SIZE = 4;
const uint16_t SENDER_INFO_SIZE = 20;
const uint16_t REPORT_BLOCK_SIZE = 24;
const uint16_t APP_NAME_SIZE = 4;

size_t get_sdes_packet_size(const std::vector<uvgrtp::frame::rtcp_sdes_item>& items);
size_t get_app_packet_size(size_t payload_len);

void construct_sdes_packet(uint8_t* frame, int& ptr,
	const std::vector<uvgrtp::frame::rtcp_sdes_item>& items);
void construct_app_packet(uint8_t* frame, int& ptr, 
	const char* name, const uint8_t* payload, size_t payload_len);