#include "h26x.hh"

#include "../rtp.hh"
#include "../queue.hh"

#include "uvgrtp/socket.hh"
#include "uvgrtp/debug.hh"


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

constexpr int GARBAGE_COLLECTION_INTERVAL_MS = 100;
constexpr int LOST_FRAME_TIMEOUT_MS = 500;

static inline unsigned __find_h26x_start(uint32_t value,bool& additional_byte)
{
    additional_byte = false;
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
        additional_byte = true;
        return 4;
    }

    return 0;
}

uvgrtp::formats::h26x::h26x(std::shared_ptr<uvgrtp::socket> socket, std::shared_ptr<uvgrtp::rtp> rtp, int flags) :
    media(socket, rtp, flags), 
    queued_(), 
    frames_(), 
    dropped_(), 
    rtp_ctx_(rtp),
    last_garbage_collection_(uvgrtp::clock::hrc::now())
{}

uvgrtp::formats::h26x::~h26x()
{}

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
            bool additional_byte =  false;
            if ((ret = start_len = __find_h26x_start(value,additional_byte)) > 0) {
                if (ret == 5) {
                    ret = 3;
#if __BYTE_ORDER == __LITTLE_ENDIAN
                    start_len = (((prev >> 24) & 0xff) == 0) ? 4 : 3;
#else
                    start_len = (((prev >>  0) & 0xff) == 0) ? 4 : 3;
#endif
                }
                if (additional_byte) start_len--;
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
        pos += get_start_code_range();
        ptr += get_start_code_range();
        prev = value;
    }

    data[rpos] = lb;
    return -1;
}

rtp_error_t uvgrtp::formats::h26x::frame_getter(uvgrtp::frame::rtp_frame** frame)
{
    if (queued_.size()) {
        *frame = queued_.front();
        queued_.pop_front();
        return RTP_PKT_READY;
    }

    return RTP_NOT_FOUND;
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
    if (data_len == 0)
        return RTP_INVALID_VALUE;

    rtp_error_t ret = RTP_OK;

    size_t payload_size = rtp_ctx_->get_payload_size();

    if (data_len <= payload_size) {
        /* If the small packet is handled we either wait for more small packets or
         * we already sent the data, so always return from this function after entering this branch */
        if ((ret = handle_small_packet(data, data_len, more)) != RTP_OK)
            return ret;
        return RTP_OK;
    }
    else {
        /* If smaller NALUs were queued before this NALU,
         * send them in an aggregation packet before proceeding with fragmentation */
        (void)make_aggregation_pkt();
    }

    size_t data_left = data_len;
    size_t data_pos = 0;

    /* The payload is larger than MTU (1500 bytes) so we must split it into 
     * smaller RTP frames, because we cannot make any assumptions about the 
     * life time of current stack, we need to store NAL and FU headers to the frame queue transaction.
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

uvgrtp::frame::rtp_frame* uvgrtp::formats::h26x::allocate_rtp_frame_with_startcode(bool add_start_code,
    uvgrtp::frame::rtp_header& header, size_t payload_size_without_startcode, size_t& fptr)
{
    uvgrtp::frame::rtp_frame* complete = uvgrtp::frame::alloc_rtp_frame();

    complete->payload_len = payload_size_without_startcode;

    if (add_start_code) {
        complete->payload_len += 4;
    } 
    
    complete->payload = new uint8_t[complete->payload_len];

    if (add_start_code) {
        complete->payload[0] = 0;
        complete->payload[1] = 0;
        complete->payload[2] = 0;
        complete->payload[3] = 1;
        fptr += 4;
    }

    complete->header = header; // copy

    return complete;
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

bool uvgrtp::formats::h26x::is_frame_late(uvgrtp::formats::h26x_info_t& hinfo, size_t max_delay)
{
    return (uvgrtp::clock::hrc::diff_now(hinfo.sframe_time) >= max_delay);
}

uint32_t uvgrtp::formats::h26x::drop_frame(uint32_t ts)
{
    uint16_t s_seq = frames_.at(ts).s_seq;
    uint16_t e_seq = frames_.at(ts).e_seq;

    LOG_INFO("Dropping frame. Ts: %u, Seq: %u - %u", ts, s_seq, e_seq);

    if (frames_.find(ts) == frames_.end())
    {
        LOG_ERROR("Tried to drop a non-existing frame");
        return 0;
    }

    uint32_t total_cleaned = 0;

    // clean fragments
    for (auto& fragment : frames_[ts].fragments) {
        total_cleaned += fragment.second->payload_len + sizeof(uvgrtp::frame::rtp_frame);
        (void)uvgrtp::frame::dealloc_frame(fragment.second);
    }
    frames_[ts].fragments.clear();

    // clean fragments that have no place
    for (auto& temporary : frames_[ts].temporary) {
        total_cleaned += temporary->payload_len + sizeof(uvgrtp::frame::rtp_frame);;
        (void)uvgrtp::frame::dealloc_frame(temporary);
    }

    // lastly, remove the frame structure from map
    frames_[ts].temporary.clear();
    frames_.erase(ts);

    dropped_.insert(ts);

    return total_cleaned;
}

rtp_error_t uvgrtp::formats::h26x::handle_aggregation_packet(uvgrtp::frame::rtp_frame** out, uint8_t nal_header_size, int flags)
{
    uvgrtp::buf_vec nalus;

    size_t size = 0;
    auto* frame = *out;

    for (size_t i = nal_header_size; i < frame->payload_len; i += ntohs(*(uint16_t*)&frame->payload[i]) + sizeof(uint16_t)) {
        nalus.push_back(
            std::make_pair(
                ntohs(*(uint16_t*)&frame->payload[i]),
                &frame->payload[i] + sizeof(uint16_t)
            )
        );

        size += ntohs(*(uint16_t*)&frame->payload[i]);
    }

    for (size_t i = 0; i < nalus.size(); ++i) {
        size_t fptr = 0;
        uvgrtp::frame::rtp_frame* retframe = 
            allocate_rtp_frame_with_startcode(flags, (*out)->header, nalus[i].first, fptr);
        
        std::memcpy(
            retframe->payload + fptr,
            nalus[i].second,
            nalus[i].first
        );

        queued_.push_back(retframe);
    }

    return RTP_MULTIPLE_PKTS_READY;
}

rtp_error_t uvgrtp::formats::h26x::packet_handler(int flags, uvgrtp::frame::rtp_frame** out)
{
    uvgrtp::frame::rtp_frame* frame;
    bool enable_idelay = !(flags & RCE_NO_H26X_INTRA_DELAY);

    /* Use "intra" to keep track of intra frames
     *
     * If uvgRTP is in the process of receiving fragments of an incomplete intra frame,
     * "intra" shall be the timestamp value of that intra frame.
     * This means that when we're receiving packets out of order and an inter frame is complete
     * while "intra" contains value other than INVALID_TS, we drop the inter frame and wait for
     * the intra frame to complete.
     *
     * If "intra" contains INVALID_TS and all packets of an inter frame have been received,
     * the inter frame is returned to user.  If intra contains a value other than INVALID_TS
     * (meaning an intra frame is in progress) and a new intra frame is received, the old intra frame
     * pointed to by "intra" and new intra frame shall take the place of active intra frame */
    uint32_t intra = INVALID_TS;

    const size_t format_header_size = get_nal_header_size() + get_fu_header_size();

    frame = *out;

    uint32_t c_ts = frame->header.timestamp;
    uint32_t c_seq = frame->header.seq;
    int frag_type = get_fragment_type(frame);
    uint8_t nal_type = get_nal_type(frame);

    if (frag_type == FT_AGGR)
        return handle_aggregation_packet(out, get_nal_header_size(), flags);

    if (frag_type == FT_NOT_FRAG) {
        prepend_start_code(flags, out);
        return RTP_PKT_READY;
    }

    if (frag_type == FT_INVALID) {
        LOG_WARN("invalid frame received!");
        (void)uvgrtp::frame::dealloc_frame(*out);
        *out = nullptr;
        return RTP_GENERIC_ERROR;
    }

    /* initialize new frame */
    if (frames_.find(c_ts) == frames_.end()) {

        /* make sure we haven't discarded the frame "c_ts" before */
        if (dropped_.find(c_ts) != dropped_.end()) {
            LOG_WARN("packet belonging to a dropped frame was received!");
            return RTP_GENERIC_ERROR;
        }

        /* drop old intra if a new one is received */
        if (nal_type == NT_INTRA) {
            if (intra != INVALID_TS && enable_idelay) {
                LOG_WARN("Dropping old h26x intra since new one has arrived");
                drop_frame(intra);
            }
            intra = c_ts;
        }

        frames_[c_ts].s_seq = INVALID_SEQ;
        frames_[c_ts].e_seq = INVALID_SEQ;

        if (frag_type == FT_START) frames_[c_ts].s_seq = c_seq;
        if (frag_type == FT_END)   frames_[c_ts].e_seq = c_seq;

        frames_[c_ts].sframe_time = uvgrtp::clock::hrc::now();
        frames_[c_ts].total_size = frame->payload_len - format_header_size;
        frames_[c_ts].pkts_received = 1;

        frames_[c_ts].fragments[c_seq] = frame;
        return RTP_OK;
    }

    frames_[c_ts].pkts_received += 1;
    frames_[c_ts].total_size += (frame->payload_len - format_header_size);

    if (frag_type == FT_START) {
        frames_[c_ts].s_seq = c_seq;
        frames_[c_ts].fragments[c_seq] = frame;

        for (auto& fragment : frames_[c_ts].temporary) {
            uint16_t fseq = fragment->header.seq;
            uint32_t seq = (c_seq > fseq) ? 0x10000 + fseq : fseq;

            frames_[c_ts].fragments[seq] = fragment;
        }
        frames_[c_ts].temporary.clear();
    }

    if (frag_type == FT_END)
        frames_[c_ts].e_seq = c_seq;

    /* Out-of-order nature poses an interesting problem when reconstructing the frame:
     * how to store the fragments such that we mustn't shuffle them around when frame reconstruction takes place?
     *
     * std::map is an option but the overflow of 16-bit sequence number counter makes that a little harder because
     * if the first few fragments of a frame are near 65535, the rest of the fragments are going to have sequence
     * numbers less than that and thus our frame reconstruction breaks.
     *
     * This can be solved by checking if current fragment's sequence is less than start fragment's sequence number
     * (overflow has occurred) and correcting the current sequence by adding 0x10000 to its value so it appears
     * in order with other fragments */
    if (frag_type != FT_START) {
        if (frames_[c_ts].s_seq != INVALID_SEQ) {
            /* overflow has occurred, adjust the sequence number of current
             * fragment so it appears in order with other fragments of the frame
             *
             * Note: if the frame is huge (~94 MB), this will not work but it's not a realistic scenario */
            frames_[c_ts].fragments[((frames_[c_ts].s_seq > c_seq) ? 0x10000 + c_seq : c_seq)] = frame;
        }
        else {
            /* position for the fragment cannot be calculated so move the fragment to a temporary storage */
            frames_[c_ts].temporary.push_back(frame);
        }
    }

    if (frames_[c_ts].s_seq != INVALID_SEQ && frames_[c_ts].e_seq != INVALID_SEQ) {
        size_t received = 0;
        size_t s_seq = frames_[c_ts].s_seq;
        size_t e_seq = frames_[c_ts].e_seq;

        if (s_seq > e_seq)
            received = 0xffff - s_seq + e_seq + 2;
        else
            received = e_seq - s_seq + 1;

        /* we've received every fragment and the frame can be reconstructed */
        if (received == frames_[c_ts].pkts_received) {

            /* intra is still in progress, do not return the inter */
            if (nal_type == NT_INTER && intra != INVALID_TS && enable_idelay) {
                LOG_WARN("Got h26x Inter frame while intra is still in progress");
                drop_frame(c_ts);
                return RTP_OK;
            }

            size_t fptr = 0;
            uvgrtp::frame::rtp_frame* complete = allocate_rtp_frame_with_startcode((flags & RCE_H26X_PREPEND_SC), 
                (*out)->header,  frames_[c_ts].total_size + get_nal_header_size(), fptr);

            copy_nal_header(fptr, frame->payload, complete->payload); // NAL header
            fptr += get_nal_header_size();

            for (auto& fragment : frames_.at(c_ts).fragments) {
                std::memcpy(
                    &complete->payload[fptr],
                    &fragment.second->payload[format_header_size],
                    fragment.second->payload_len - format_header_size
                );
                fptr += fragment.second->payload_len - format_header_size;
                (void)uvgrtp::frame::dealloc_frame(fragment.second);
            }


            if (nal_type == NT_INTRA)
                intra = INVALID_TS;

            *out = complete;
            frames_.erase(c_ts);
            return RTP_PKT_READY;
        }
    }

    if (is_frame_late(frames_.at(c_ts), rtp_ctx_->get_pkt_max_delay())) {
        if (nal_type != NT_INTRA || (nal_type == NT_INTRA && !enable_idelay)) {
            LOG_WARN("Received a packet that is too late!");
            drop_frame(c_ts);
        }
    }

    garbage_collect_lost_frames();

    return RTP_OK;
}

void uvgrtp::formats::h26x::copy_nal_header(size_t fptr, uint8_t* frame_payload, uint8_t* complete_payload)
{
    uint8_t nal_header[2] = {
        (uint8_t)((frame_payload[0] & 0x81) | ((frame_payload[2] & 0x3f) << 1)),
        (uint8_t)frame_payload[1]
    };

    std::memcpy(&complete_payload[fptr], nal_header, get_nal_header_size());
}

void uvgrtp::formats::h26x::garbage_collect_lost_frames()
{
    if (uvgrtp::clock::hrc::diff_now(last_garbage_collection_) >= GARBAGE_COLLECTION_INTERVAL_MS) {
        uint32_t total_cleaned = 0;
        std::vector<uint32_t> to_remove;

        // first find all frames that have been waiting for too long
        for (auto& gc_frame : frames_) {
            if (uvgrtp::clock::hrc::diff_now(gc_frame.second.sframe_time) > LOST_FRAME_TIMEOUT_MS) {
                LOG_WARN("Found an old frame that has not been completed");
                to_remove.push_back(gc_frame.first);
            }
        }

        // remove old frames
        for (auto& old_frame : to_remove) {

            total_cleaned += drop_frame(old_frame);
        }

        if (total_cleaned > 0) {
            LOG_INFO("Garbage collection cleaned %d bytes!", total_cleaned);
        }

        last_garbage_collection_ = uvgrtp::clock::hrc::now();
    }
}