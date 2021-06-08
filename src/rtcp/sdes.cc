#include "rtcp.hh"

#include "../srtp/srtcp.hh"
#include "debug.hh"

uvgrtp::frame::rtcp_sdes_packet *uvgrtp::rtcp::get_sdes_packet(uint32_t ssrc)
{
    if (participants_.find(ssrc) == participants_.end())
        return nullptr;

    auto frame = participants_[ssrc]->sdes_frame;
    participants_[ssrc]->sdes_frame = nullptr;

    return frame;
}

rtp_error_t uvgrtp::rtcp::install_sdes_hook(void (*hook)(uvgrtp::frame::rtcp_sdes_packet *))
{
    if (!hook)
        return RTP_INVALID_VALUE;

    sdes_hook_ = hook;
    return RTP_OK;
}

rtp_error_t uvgrtp::rtcp::handle_sdes_packet(uint8_t *packet, size_t size)
{
    if (!packet || !size)
        return RTP_INVALID_VALUE;

    auto frame = new uvgrtp::frame::rtcp_sdes_packet;
    auto ret   = RTP_OK;

    frame->header.version = (packet[0] >> 6) & 0x3;
    frame->header.padding = (packet[0] >> 5) & 0x1;
    frame->header.count   = packet[0] & 0x1f;
    frame->header.length  = ntohs(*(uint16_t *)&packet[2]);
    frame->ssrc           = ntohl(*(uint32_t *)&packet[4]);

    if (srtcp_ && (ret = srtcp_->handle_rtcp_decryption(flags_, frame->ssrc, packet, size)) != RTP_OK)
        return ret;

    /* Deallocate previous frame from the buffer if it exists, it's going to get overwritten */
    if (participants_[frame->ssrc]->sdes_frame) {
        for (auto& item : participants_[frame->ssrc]->sdes_frame->items)
            delete[] (uint8_t *)item.data;
        delete participants_[frame->ssrc]->sdes_frame;
    }

    for (int ptr = 8; ptr < frame->header.length; ) {
        uvgrtp::frame::rtcp_sdes_item item;

        item.type   = packet[ptr++];
        item.length = packet[ptr++];
        item.data   = (void *)new uint8_t[item.length];

        memcpy(item.data, &packet[ptr], item.length);
        ptr += item.length;
    }

    if (sdes_hook_)
        sdes_hook_(frame);
    else
        participants_[frame->ssrc]->sdes_frame = frame;

    return RTP_OK;
}

rtp_error_t uvgrtp::rtcp::send_sdes_packet(std::vector<uvgrtp::frame::rtcp_sdes_item>& items)
{
    if (items.empty()) {
        LOG_ERROR("Cannot send an empty SDES packet!");
        return RTP_INVALID_VALUE;
    }

    if (num_receivers_ > 31) {
        LOG_ERROR("Source count is larger than packet supports!");

        // TODO: Multiple SDES packets should be sent in this case
        return RTP_GENERIC_ERROR;
    }

    int ptr = 8;
    uint8_t *frame = nullptr;
    rtp_error_t ret = RTP_OK;
    size_t frame_size = 0;

    frame_size  = 4 + 4;            /* rtcp header + ssrc */
    frame_size += items.size() * 2; /* sdes item type + length */

    for (auto& item : items)
        frame_size += item.length;

    construct_rtcp_header(frame_size, frame, num_receivers_, uvgrtp::frame::RTCP_FT_SDES, true);

    for (auto& item : items) {
        frame[ptr++] = item.type;
        frame[ptr++] = item.length;
        memcpy(frame + ptr, item.data, item.length);
        ptr += item.length;
    }

    if (srtcp_ && (ret = srtcp_->handle_rtcp_encryption(flags_, rtcp_pkt_sent_count_, ssrc_, frame, frame_size)) != RTP_OK)
        return ret;

    for (auto& p : participants_) {
        if ((ret = p.second->socket->sendto(p.second->address, frame, frame_size, 0)) != RTP_OK) {
            LOG_ERROR("sendto() failed!");
            goto end;
        }
        update_rtcp_bandwidth(frame_size);
    }

end:
    delete[] frame;
    return ret;
}
