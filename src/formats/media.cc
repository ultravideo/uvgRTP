#ifdef __linux__
#else
#endif

#include "formats/media.hh"

uvg_rtp::formats::media::media(uvg_rtp::socket *socket, uvg_rtp::rtp *rtp_ctx, int flags):
    socket_(socket), rtp_ctx_(rtp_ctx), flags_(flags)
{
}

uvg_rtp::formats::media::~media()
{
}

rtp_error_t uvg_rtp::formats::media::push_frame(uint8_t *data, size_t data_len, int flags)
{
    if (!data || !data_len)
        return RTP_INVALID_VALUE;

    return __push_frame(data, data_len, flags);
}

rtp_error_t uvg_rtp::formats::media::push_frame(std::unique_ptr<uint8_t[]> data, size_t data_len, int flags)
{
    if (!data || !data_len)
        return RTP_INVALID_VALUE;

    return __push_frame(data.get(), data_len, flags);
}

rtp_error_t uvg_rtp::formats::media::__push_frame(uint8_t *data, size_t data_len, int flags)
{
    std::vector<std::pair<size_t, uint8_t *>> buffers;
    size_t payload_size = rtp_ctx_->get_payload_size();
    uint8_t header[uvg_rtp::frame::HEADER_SIZE_RTP];

    /* fill RTP header with our session's values
     * and push the header to the buffer vector to use vectored I/O */
    rtp_ctx_->fill_header(header);
    buffers.push_back(std::make_pair(sizeof(header), header));
    buffers.push_back(std::make_pair(data_len,       data));

    if (data_len > payload_size) {
        if (flags_ & RCE_FRAGMENT_GENERIC) {

            rtp_error_t ret   = RTP_OK;
            ssize_t data_left = data_len;
            ssize_t data_pos  = 0;

            /* set marker bit for the first fragment */
            header[1] |= (1 << 7);

            while (data_left > (ssize_t)payload_size) {
                buffers.at(1).first  = payload_size;
                buffers.at(1).second = data + data_pos;

                if ((ret = socket_->sendto(buffers, 0)) != RTP_OK)
                    return ret;

                rtp_ctx_->update_sequence(header);

                data_pos  += payload_size;
                data_left -= payload_size;

                /* clear marker bit for middle fragments */
                header[1] &= 0x7f;
            }

            /* set marker bit for the last frame */
            header[1] |= (1 << 7);

            buffers.at(1).first  = data_left;
            buffers.at(1).second = data + data_pos;

            return socket_->sendto(buffers, 0);

        } else {
            LOG_WARN("Packet is larger (%zu bytes) than payload_size (%zu bytes)", data_len, payload_size);
        }
    }

    return socket_->sendto(buffers, 0);
}

static rtp_error_t packet_handler(ssize_t size, void *packet, int flags, uvg_rtp::frame::rtp_frame **out)
{
    (void)size, (void)packet;
    return RTP_OK;
}
