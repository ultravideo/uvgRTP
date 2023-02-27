#include "rtcp_packets.hh"

#include "srtp/srtcp.hh"

#include "debug.hh"
#include <memory>
#define SET_NEXT_FIELD_32(a, p, v) do { *(uint32_t *)&(a)[p] = (v); p += 4; } while (0)

uint32_t uvgrtp::get_sr_packet_size(int rce_flags, uint16_t reports)
{
    /* Sender report is otherwise identical with receiver report, 
     * but it also includes sender info */
    return get_rr_packet_size(rce_flags, reports) + SENDER_INFO_SIZE;
}

uint32_t uvgrtp::get_rr_packet_size(int rce_flags, uint16_t reports)
{
    uint32_t size = (size_t)RTCP_HEADER_SIZE + SSRC_CSRC_SIZE
        + (size_t)REPORT_BLOCK_SIZE * reports;
    if (rce_flags & RCE_SRTP)
    {
        size += UVG_SRTCP_INDEX_LENGTH + UVG_AUTH_TAG_LENGTH;
    }

    return size;
}

uint32_t uvgrtp::get_app_packet_size(uint32_t payload_len)
{
    return RTCP_HEADER_SIZE + SSRC_CSRC_SIZE + APP_NAME_SIZE + payload_len;
}

uint32_t uvgrtp::get_sdes_packet_size(const std::vector<uvgrtp::frame::rtcp_sdes_item>& items) {
    /* We currently only support having one source. If uvgRTP is used in a mixer, multiple sources
     * should be supported in SDES packet. */

    uint32_t frame_size = RTCP_HEADER_SIZE + SSRC_CSRC_SIZE; // our ssrc
    frame_size += (uint32_t)items.size() * 2; /* sdes item type + length, both take one byte */
    for (auto& item : items)
    {
        frame_size += item.length;
    }

    /* each chunk must end to a zero octet so 4 zeros is only option
     * if the length matches 32-bits multiples */
    frame_size += (4 - frame_size % 4);

    return frame_size;
}

uint32_t uvgrtp::get_bye_packet_size(const std::vector<uint32_t>& ssrcs)
{
    return RTCP_HEADER_SIZE + (uint32_t)ssrcs.size() * SSRC_CSRC_SIZE;
}

bool uvgrtp::construct_rtcp_header(uint8_t* frame, size_t& ptr, size_t packet_size,
    uint8_t secondField, uvgrtp::frame::RTCP_FRAME_TYPE frame_type)
{
    if (packet_size > UINT16_MAX)
    {
        UVG_LOG_ERROR("RTCP receiver report packet size too large!");
        return false;
    }

    if (packet_size % 4 != 0)
    {
        UVG_LOG_ERROR("RTCP packet size should be measured in 32-bit words!");
        return false;
    }

    uint8_t v = 2;
    uint8_t p = 0;

    // header |V=2|P|    SC   |  PT  |             length            |
    frame[ptr] = (v << 6) | (p << 5) | secondField;
    frame[ptr + 1] = (uint8_t)frame_type;

    // The RTCP header length field is measured in 32-bit words - 1
    *(uint16_t*)&frame[ptr + 2] = htons((uint16_t)packet_size / sizeof(uint32_t) - 1);
    ptr += RTCP_HEADER_SIZE;

    return true;
}

bool uvgrtp::construct_ssrc(uint8_t* frame, size_t& ptr, uint32_t ssrc)
{
    SET_NEXT_FIELD_32(frame, ptr, htonl(ssrc));
    return true;
}

bool uvgrtp::construct_sender_info(uint8_t* frame, size_t& ptr, uint64_t ntp_ts, uint64_t rtp_ts,
    uint32_t sent_packets, uint32_t sent_bytes)
{
    SET_NEXT_FIELD_32(frame, ptr, htonl(ntp_ts >> 32));        // NTP ts msw
    SET_NEXT_FIELD_32(frame, ptr, htonl(ntp_ts & 0xffffffff)); // NTP ts lsw
    SET_NEXT_FIELD_32(frame, ptr, htonl((uint32_t)rtp_ts));    // RTP ts
    SET_NEXT_FIELD_32(frame, ptr, htonl(sent_packets));        // sender's packet count
    SET_NEXT_FIELD_32(frame, ptr, htonl(sent_bytes));          // sender's octet count (not bytes)

    return true;
}

bool uvgrtp::construct_report_block(uint8_t* frame, size_t& ptr, uint32_t ssrc, uint8_t fraction,
    uint32_t dropped_packets, uint16_t seq_cycles, uint16_t max_seq, uint32_t jitter, 
    uint32_t lsr, uint32_t dlsr)
{
    SET_NEXT_FIELD_32(frame, ptr, htonl(ssrc));
    SET_NEXT_FIELD_32(frame, ptr, htonl(uint32_t(fraction << 24) | dropped_packets));
    SET_NEXT_FIELD_32(frame, ptr, htonl(uint32_t(seq_cycles) << 16 | max_seq));
    SET_NEXT_FIELD_32(frame, ptr, htonl(jitter));
    SET_NEXT_FIELD_32(frame, ptr, htonl(lsr));
    SET_NEXT_FIELD_32(frame, ptr, htonl(dlsr));

    return true;
}

bool uvgrtp::construct_app_packet(uint8_t* frame, size_t& ptr,
    const char* name, std::unique_ptr<uint8_t[]> payload, size_t payload_len)
{
    memcpy(&frame[ptr], name, APP_NAME_SIZE);
    memcpy(&frame[ptr + APP_NAME_SIZE], payload.get(), payload_len);
    ptr += APP_NAME_SIZE + payload_len;

    return true;
}

bool uvgrtp::construct_sdes_chunk(uint8_t* frame, size_t& ptr,
    uvgrtp::frame::rtcp_sdes_chunk chunk) {

    bool have_cname = false;

    construct_ssrc(frame, ptr, chunk.ssrc);

    for (auto& item : chunk.items)
    {
        if (item.type == 1)
        {
            have_cname = true;
        }

        frame[ptr++] = item.type;
        frame[ptr++] = item.length;
        memcpy(frame + ptr, item.data, item.length);
        ptr += item.length;
    }

    ptr += (4 - ptr % 4);

    if (!have_cname)
    {
        UVG_LOG_ERROR("SDES chunk did not contain cname!");
    }

    return have_cname;
}

bool uvgrtp::construct_bye_packet(uint8_t* frame, size_t& ptr, const std::vector<uint32_t>& ssrcs)
{
    for (auto& ssrc : ssrcs)
    {
        SET_NEXT_FIELD_32(frame, ptr, htonl(ssrc));
    }

    return true;
}

bool uvgrtp::construct_app_block(uint8_t* frame, size_t& write_ptr, uint8_t sec_field, uint32_t ssrc, const char* name, std::unique_ptr<uint8_t[]> payload, size_t payload_len)
{
    uint32_t packet_size = get_app_packet_size((uint32_t)payload_len);
    return construct_rtcp_header(frame, write_ptr, packet_size, sec_field,
        uvgrtp::frame::RTCP_FT_APP) &&
        construct_ssrc(frame, write_ptr, ssrc) &&
        construct_app_packet(frame, write_ptr, name, std::move(payload), payload_len);
}
