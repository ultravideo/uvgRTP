#include "h26x.hh"

#include "../rtp.hh"
#include "../queue.hh"
#include "socket.hh"
#include "debug.hh"


#include <cstdint>
#include <cstring>
#include <iostream>
#include <unordered_map>
#include <queue>


#ifndef _WIN32
#include <sys/socket.h>
#endif


#define PTR_DIFF(a, b)  ((ptrdiff_t)((char *)(a) - (char *)(b)))

#define haszero64_le(v) (((v) - 0x0101010101010101) & ~(v) & 0x8080808080808080UL)
#define haszero32_le(v) (((v) - 0x01010101)         & ~(v) & 0x80808080UL)

#define haszero64_be(v) (((v) - 0x1010101010101010) & ~(v) & 0x0808080808080808UL)
#define haszero32_be(v) (((v) - 0x10101010)         & ~(v) & 0x08080808UL)

#ifndef __LITTLE_ENDIAN
#define __LITTLE_ENDIAN 1337
#endif

#ifndef __BYTE_ORDER
#define __BYTE_ORDER __LITTLE_ENDIAN
#endif

static inline unsigned __find_h26x_start(uint32_t value)
{
#if __BYTE_ORDER == __LITTLE_ENDIAN
    uint16_t u = (value >> 16) & 0xffff;
    uint16_t l = (value >>  0) & 0xffff;

    bool t1 = (l == 0);
    bool t2 = ((u & 0xff) == 0x01);
    bool t3 = (u == 0x0100);
    bool t4 = (((l >> 8) & 0xff) == 0);
#else
    uint16_t u = (value >>  0) & 0xffff;
    uint16_t l = (value >> 16) & 0xffff;

    bool t1 = (l == 0);
    bool t2 = (((u >> 8) & 0xff) == 0x01);
    bool t3 = (u == 0x0001);
    bool t4 = ((l & 0xff) == 0);
#endif

    if (t1) {
        /* 0x00000001 */
        if (t3)
            return 4;

        /* "value" definitely has a start code (0x000001XX), but at this
         * point we can't know for sure whether it's 3 or 4 bytes long.
         *
         * Return 5 to indicate that start length could not be determined
         * and that caller must check previous dword's last byte for 0x00 */
        if (t2)
            return 5;
    } else if (t4 && t3) {
        /* 0xXX000001 */
        return 4;
    }

    return 0;
}

uvgrtp::formats::h26x::h26x(uvgrtp::socket* socket, uvgrtp::rtp* rtp, int flags) :
    media(socket, rtp, flags)
{
}

uvgrtp::formats::h26x::~h26x()
{
    delete fqueue_;
}

/* NOTE: the area 0 - len (ie data[0] - data[len - 1]) must be addressable
 * Do not add offset to "data" ptr before passing it to find_h26x_start_code()! */
ssize_t uvgrtp::formats::h26x::find_h26x_start_code(
    uint8_t *data,
    size_t len,
    size_t offset,
    uint8_t& start_len
)
{
    bool prev_z   = false;
    bool cur_z    = false;
    size_t pos    = offset;
    size_t rpos   = len - (len % 8) - 1;
    uint8_t *ptr  = data + offset;
    uint8_t *tmp  = nullptr;
    uint8_t lb    = 0;
    uint32_t prev = UINT32_MAX;

    uint64_t prefetch = UINT64_MAX;
    uint32_t value    = UINT32_MAX;
    unsigned ret      = 0;

    /* We can get rid of the bounds check when looping through
     * non-zero 8 byte chunks by setting the last byte to zero.
     *
     * This added zero will make the last 8 byte zero check to fail
     * and when we get out of the loop we can check if we've reached the end */
    lb = data[rpos];
    data[rpos] = 0;

    while (pos + 8 < len) {
        prefetch = *(uint64_t *)ptr;

#if __BYTE_ORDER == __LITTLE_ENDIAN
        if (!prev_z && !(cur_z = haszero64_le(prefetch))) {
#else
        if (!prev_z && !(cur_z = haszero64_be(prefetch))) {
#endif
            /* pos is not used in the following loop so it makes little sense to
             * update it on every iteration. Faster way to do the loop is to save
             * ptr's current value before loop, update only ptr in the loop and when
             * the loop is exited, calculate the difference between tmp and ptr to get
             * the number of iterations done * 8 */
            tmp = ptr;

            do {
                ptr      += 8;
                prefetch  = *(uint64_t *)ptr;
#if __BYTE_ORDER == __LITTLE_ENDIAN
                cur_z     = haszero64_le(prefetch);
#else
                cur_z     = haszero64_be(prefetch);
#endif
            } while (!cur_z);

            pos += PTR_DIFF(ptr, tmp);

            if (pos + 8 >= len)
                break;
        }

        value = *(uint32_t *)ptr;

        if (cur_z)
#if __BYTE_ORDER == __LITTLE_ENDIAN
            cur_z = haszero32_le(value);
#else
            cur_z = haszero32_be(value);
#endif

        if (!prev_z && !cur_z)
            goto end;

        /* Previous dword had zeros but this doesn't. The only way there might be a start code
         * is if the most significant byte of current dword is 0x01 */
        if (prev_z && !cur_z) {
#if __BYTE_ORDER == __LITTLE_ENDIAN
            /* previous dword: 0xXX000000 or 0xXXXX0000 and current dword 0x01XXXXXX */
            if (((value  >> 0) & 0xff) == 0x01 && ((prev >> 16) & 0xffff) == 0) {
                start_len = (((prev >>  8) & 0xffffff) == 0) ? 4 : 3;
#else
            if (((value >> 24) & 0xff) == 0x01 && ((prev >>  0) & 0xffff) == 0) {
                start_len = (((prev >>  0) & 0xffffff) == 0) ? 4 : 3;
#endif
                data[rpos] = lb;
                return pos + 1;
            }
        }


        {
            if ((ret = start_len = __find_h26x_start(value)) > 0) {
                if (ret == 5) {
                    ret = 3;
#if __BYTE_ORDER == __LITTLE_ENDIAN
                    start_len = (((prev >> 24) & 0xff) == 0) ? 4 : 3;
#else
                    start_len = (((prev >>  0) & 0xff) == 0) ? 4 : 3;
#endif
                }

                data[rpos] = lb;
                return pos + ret;
            }

#if __BYTE_ORDER == __LITTLE_ENDIAN
            uint16_t u = (value >> 16) & 0xffff;
            uint16_t l = (value >>  0) & 0xffff;
            uint16_t p = (prev  >> 16) & 0xffff;

            bool t1 = ((p & 0xffff) == 0);
            bool t2 = (((p >> 8) & 0xff) == 0);
            bool t4 = (l == 0x0100);
            bool t5 = (l == 0x0000 && u == 0x01);
#else
            uint16_t u = (value >>  0) & 0xffff;
            uint16_t l = (value >> 16) & 0xffff;
            uint16_t p = (prev  >>  0) & 0xffff;

            bool t1 = ((p & 0xffff) == 0);
            bool t2 = ((p & 0xff) == 0);
            bool t4 = (l == 0x0001);
            bool t5 = (l == 0x0000 && u == 0x01);
#endif
            if (t1 && t4) {
                /* previous dword 0xxxxx0000 and current dword is 0x0001XXXX */
                if (t4) {
                    start_len = 4;
                    data[rpos] = lb;
                    return pos + 2;
                }
            /* Previous dwod was 0xXXXXXX00 */
            } else if (t2) {
                /* Current dword is 0x000001XX */
                if (t5) {
                    start_len = 4;
                    data[rpos] = lb;
                    return pos + 3;
                }

                /* Current dword is 0x0001XXXX */
                else if (t4) {
                    start_len = 3;
                    data[rpos] = lb;
                    return pos + 2;
                }
            }

        }
end:
        prev_z = cur_z;
        pos += 4;
        ptr += 4;
        prev = value;
    }

    data[rpos] = lb;
    return -1;
}

rtp_error_t uvgrtp::formats::h26x::push_h26x_frame(uint8_t *data, size_t data_len, int flags)
{
    /* find first start code */
    uint8_t start_len   = 0;
    ssize_t offset          = find_h26x_start_code(data, data_len, 0, start_len);
    ssize_t prev_offset     = offset;
    size_t r_off        = 0;
    rtp_error_t ret     = RTP_GENERIC_ERROR;
    size_t payload_size = rtp_ctx_->get_payload_size();

    if (data_len < payload_size || flags & RTP_SLICE) {
        r_off = (offset < 0) ? 0 : offset;

        if (data_len > payload_size) {
            return push_nal_unit(data + r_off, data_len, false);
        } else {
            if ((ret = fqueue_->enqueue_message(data + r_off, data_len - r_off)) != RTP_OK) {
                LOG_ERROR("Failed to enqueue Single h26x NAL Unit packet!");
                return ret;
            }

            return fqueue_->flush_queue();
        }
    }

    while (offset != -1) {
        offset = find_h26x_start_code(data, data_len, offset, start_len);

        if (offset != -1) {
            ret = push_nal_unit(&data[prev_offset], offset - prev_offset - start_len, true);

            if (ret != RTP_NOT_READY)
                goto error;

            prev_offset = offset;
        }
    }

    if (prev_offset == -1)
        prev_offset = 0;

    if ((ret = push_nal_unit(&data[prev_offset], data_len - prev_offset, false)) == RTP_OK)
        return RTP_OK;

error:
    fqueue_->deinit_transaction();
    return ret;
}

rtp_error_t uvgrtp::formats::h26x::push_nal_unit(uint8_t *data, size_t data_len, bool more)
{
    if (data_len <= 3)
        return RTP_INVALID_VALUE;

    rtp_error_t ret = RTP_OK;

    size_t payload_size = rtp_ctx_->get_payload_size();

    if (data_len - 3 <= payload_size) {
        if ((ret = handle_small_packet(data, data_len, more)) != RTP_OK)
            return ret;
    }
    else {
        /* If smaller NALUs were queued before this NALU,
         * send them in an aggregation packet before proceeding with fragmentation */
        (void)make_aggregation_pkt();
    }

    size_t data_left = data_len;
    size_t data_pos = 0;

    /* The payload is larger than MTU (1500 bytes) so we must split it into smaller RTP frames
     * Because we don't if the SCD is enabled and thus cannot make any assumptions about the life time
     * of current stack, we need to store NAL and FU headers to the frame queue transaction.
     *
     * This can be done by asking a handle to current transaction's buffer vectors.
     *
     * During Connection initialization, the frame queue was given the payload format so the
     * transaction also contains our media-specific headers [get_media_headers()]. */
    uvgrtp::buf_vec buffers = fqueue_->get_buffer_vector();

    if ((ret = construct_format_header_divide_fus(data, data_left, data_pos, payload_size, buffers)) != RTP_OK)
        return ret;

    if ((ret = fqueue_->enqueue_message(buffers)) != RTP_OK) {
        LOG_ERROR("Failed to send HEVC frame!");
        clear_aggregation_info();
        fqueue_->deinit_transaction();
        return ret;
    }

    if (more)
        return RTP_NOT_READY;

    clear_aggregation_info();
    return fqueue_->flush_queue();
}

rtp_error_t uvgrtp::formats::h26x::push_media_frame(uint8_t *data, size_t data_len, int flags)
{
    rtp_error_t ret;

    if (!data || !data_len)
        return RTP_INVALID_VALUE;

    if ((ret = fqueue_->init_transaction(data)) != RTP_OK) {
        LOG_ERROR("Invalid frame queue or failed to initialize transaction!");
        return ret;
    }

    return push_h26x_frame(data, data_len, flags);
}


rtp_error_t uvgrtp::formats::h26x::make_aggregation_pkt()
{
    return RTP_OK;
}

void uvgrtp::formats::h26x::clear_aggregation_info()
{}

rtp_error_t uvgrtp::formats::h26x::divide_frame_to_fus(uint8_t* data, size_t& data_left, size_t& data_pos, size_t payload_size,
    uvgrtp::buf_vec& buffers, uint8_t fu_headers[])
{
    rtp_error_t ret = RTP_OK;

    while (data_left > payload_size) {
        buffers.at(2).first = payload_size;
        buffers.at(2).second = &data[data_pos];

        if ((ret = fqueue_->enqueue_message(buffers)) != RTP_OK) {
            LOG_ERROR("Queueing the message failed!");
            clear_aggregation_info();
            fqueue_->deinit_transaction();
            return ret;
        }

        data_pos += payload_size;
        data_left -= payload_size;

        /* from now on, use the FU header meant for middle fragments */
        buffers.at(1).second = &fu_headers[1];
    }

    /* use the FU header meant for the last fragment */
    buffers.at(1).second = &fu_headers[2];

    buffers.at(2).first = data_left;
    buffers.at(2).second = &data[data_pos];

    return ret;
}

void uvgrtp::formats::h26x::initialize_fu_headers(uint8_t nal_type, uint8_t fu_headers[])
{
    fu_headers[0] = (uint8_t)((1 << 7) | nal_type);
    fu_headers[1] = nal_type;
    fu_headers[2] = (uint8_t)((1 << 6) | nal_type);
}

void uvgrtp::formats::h26x::prepend_start_code(int flags, uvgrtp::frame::rtp_frame** out)
{
    if (flags & RCE_H26X_PREPEND_SC) {
        uint8_t* pl = new uint8_t[(*out)->payload_len + 4];

        pl[0] = 0;
        pl[1] = 0;
        pl[2] = 0;
        pl[3] = 1;

        std::memcpy(pl + 4, (*out)->payload, (*out)->payload_len);
        delete[](*out)->payload;

        (*out)->payload = pl;
        (*out)->payload_len += 4;
    }
}