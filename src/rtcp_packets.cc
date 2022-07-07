#include "rtcp_packets.hh"

#include "uvgrtp/debug.hh"


size_t get_sdes_packet_size(const std::vector<uvgrtp::frame::rtcp_sdes_item>& items) {
    size_t frame_size = 0;

    /* We currently only support having one source. If uvgRTP is used in a mixer, multiple sources
     * should be supported in SDES packet. */

     // calculate SDES packet size
    frame_size = RTCP_HEADER_SIZE + SSRC_CSRC_SIZE; // our csrc
    frame_size += items.size() * 2; /* sdes item type + length, both take one byte */
    for (auto& item : items)
    {
        if (item.length <= 255)
        {
            frame_size += item.length;
        }
        else
        {
            LOG_ERROR("SDES item text must not be longer than 255 characters");
        }
    }

    return frame_size;
}

size_t get_app_packet_size(size_t payload_len)
{
    return RTCP_HEADER_SIZE + SSRC_CSRC_SIZE + APP_NAME_SIZE + payload_len;
}

void construct_sdes_packet(uint8_t* frame, int& ptr,
	const std::vector<uvgrtp::frame::rtcp_sdes_item>& items) {
    for (auto& item : items)
    {
        if (item.length <= 255)
        {
            frame[ptr++] = item.type;
            frame[ptr++] = item.length;
            memcpy(frame + ptr, item.data, item.length);
            ptr += item.length;
        }
    }
}

void construct_app_packet(uint8_t* frame, int& ptr, 
    const char* name, const uint8_t* payload, size_t payload_len)
{
    memcpy(&frame[RTCP_HEADER_SIZE + SSRC_CSRC_SIZE], name, APP_NAME_SIZE);
    memcpy(&frame[RTCP_HEADER_SIZE + SSRC_CSRC_SIZE + APP_NAME_SIZE], payload, payload_len);
}