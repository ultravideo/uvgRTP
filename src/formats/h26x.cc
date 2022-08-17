#include "h26x.hh"

#include "uvgrtp/socket.hh"

#include "../rtp.hh"
#include "../frame_queue.hh"
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

constexpr int GARBAGE_COLLECTION_INTERVAL_MS = 100;

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
    fragments_(UINT16_MAX + 1, nullptr),
    dropped_(), 
    rtp_ctx_(rtp),
    last_garbage_collection_(uvgrtp::clock::hrc::now()),
    discard_until_key_frame_(true)
{}

uvgrtp::formats::h26x::~h26x()
{
    for (auto& frame : queued_)
    {
        delete[] frame;
    }

    queued_.clear();
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

rtp_error_t uvgrtp::formats::h26x::push_media_frame(uint8_t* data, size_t data_len, int flags)
{
    rtp_error_t ret = RTP_OK;

    if (!data || !data_len)
        return RTP_INVALID_VALUE;

    if ((ret = fqueue_->init_transaction(data)) != RTP_OK) {
        UVG_LOG_ERROR("Invalid frame queue or failed to initialize transaction!");
        return ret;
    }

    size_t payload_size = rtp_ctx_->get_payload_size();

    // find all the locations of NAL units using Start Code Lookup (SCL)
    std::vector<nal_info> nals;
    bool should_aggregate = false;

    if (flags & RTP_NO_H26X_SCL) {
        nal_info nal;
        nal.offset = 0;
        nal.prefix_len = 0;
        nal.size = data_len;
        nal.aggregate = false;

        nals.push_back(nal);
    }
    else {
        scl(data, data_len, payload_size, nals, should_aggregate);
    }

    if (nals.empty())
    {
        UVG_LOG_ERROR("Did not find any NAL units in frame. Cannot send.");
        return RTP_INVALID_VALUE;
    }

    if (should_aggregate) // an aggregate packet is possible
    {
        // use aggregation function that also may just send the packets as Single NAL units 
        // if aggregates have not been implemented

        for (auto& nal : nals)
        {
            if (nal.aggregate)
            {
                if ((ret = add_aggregate_packet(data + nal.offset, nal.size)) != RTP_OK)
                {
                    clear_aggregation_info();
                    fqueue_->deinit_transaction();
                    return ret;
                }
            }
        }

        (void)finalize_aggregation_pkt();
    }

    for (auto& nal : nals) // non-aggregatable NAL units
    {
        if (!nal.aggregate || !should_aggregate)
        {
            // single NAL unit uses the NAL unit header as the payload header meaning that it does not
            // add anything extra to the packet and we can just compare the NAL size with the payload size allowed
            if (nal.size <= payload_size) // send as a single NAL unit packet
            {
                ret = single_nal_unit(data + nal.offset, nal.size);
            }
            else // send divided based on payload_size
            {
                ret = fu_division(&data[nal.offset], nal.size, payload_size);
            }

            if (ret != RTP_OK)
            {
                clear_aggregation_info();
                fqueue_->deinit_transaction();
                return ret;
            }
        }
    }

    // actually send the packets
    ret = fqueue_->flush_queue();
    clear_aggregation_info();

    return ret;
}

rtp_error_t uvgrtp::formats::h26x::add_aggregate_packet(uint8_t* data, size_t data_len)
{
    // the default implementation is to just use single NAL units and don't do the aggregate packet
    return single_nal_unit(data, data_len);
}

rtp_error_t uvgrtp::formats::h26x::finalize_aggregation_pkt()
{
    return RTP_OK;
}

void uvgrtp::formats::h26x::clear_aggregation_info()
{}

rtp_error_t uvgrtp::formats::h26x::single_nal_unit(uint8_t* data, size_t data_len)
{
    // single NAL unit packets use NAL header directly as payload header so the packet is
    // correct as is
    rtp_error_t ret = RTP_OK;
    if ((ret = fqueue_->enqueue_message(data, data_len)) != RTP_OK) {
        UVG_LOG_ERROR("Failed to enqueue single h26x NAL Unit packet!");
    }

    return ret;
}

rtp_error_t uvgrtp::formats::h26x::divide_frame_to_fus(uint8_t* data, size_t& data_left, 
    size_t payload_size, uvgrtp::buf_vec& buffers, uint8_t fu_headers[])
{
    if (data_left <= payload_size)
    {
        UVG_LOG_ERROR("Cannot use FU division for packets smaller than payload size");
        return RTP_GENERIC_ERROR;
    }

    rtp_error_t ret = RTP_OK;

    // the FU structure has both payload header and an fu header
    size_t fu_payload_size = payload_size - get_payload_header_size() - get_fu_header_size();

    // skip NAL header of data since it is incorporated in payload and fu headers (which are repeated
    // for each packet, but NAL header is only at the beginning of NAL unit)
    size_t data_pos = get_nal_header_size();
    data_left -= get_nal_header_size();

    while (data_left > fu_payload_size) {

        /* This seems to work by always using the payload headers in first and fu headers in the second index 
         * of buffer (and modifying those) and replacing the payload in third, then sending all. 
         * The headers for first fragment are already in buffers.at(1) */

        // set the payload for this fragment
        buffers.at(2).first = fu_payload_size;
        buffers.at(2).second = &data[data_pos];

        if ((ret = fqueue_->enqueue_message(buffers)) != RTP_OK) {
            UVG_LOG_ERROR("Queueing the FU packet failed!");
            return ret;
        }

        data_pos += fu_payload_size;
        data_left -= fu_payload_size;

        buffers.at(1).second = &fu_headers[1]; // middle fragment header
    }

    buffers.at(1).second = &fu_headers[2]; // last fragment header

    // set payload for the last fragment
    buffers.at(2).first = data_left;
    buffers.at(2).second = &data[data_pos];

    // send the last fragment
    if ((ret = fqueue_->enqueue_message(buffers)) != RTP_OK) {
        UVG_LOG_ERROR("Failed to send the last fragment of an H26x frame!");
    }

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
    uint32_t total_cleaned = 0;
    if (frames_.find(ts) == frames_.end())
    {
        UVG_LOG_ERROR("Tried to drop a non-existing frame");
        return total_cleaned;
    }

    /*
    uint16_t s_seq = frames_.at(ts).s_seq;
    uint16_t e_seq = frames_.at(ts).e_seq;

    UVG_LOG_INFO("Dropping frame. Ts: %lu, Seq: %u <-> %u, received/expected: %lli/%lli", 
        ts, s_seq, e_seq, frames_[ts].received_packet_seqs.size(), calculate_expected_fus(ts));
    */

    for (auto& fragment_seq : frames_[ts].received_packet_seqs)
    {
        total_cleaned += fragments_[fragment_seq]->payload_len + sizeof(uvgrtp::frame::rtp_frame);
        free_fragment(fragment_seq);
    }

    frames_.erase(ts);
    dropped_.insert(ts);

    discard_until_key_frame_ = true;

    return total_cleaned;
}

rtp_error_t uvgrtp::formats::h26x::handle_aggregation_packet(uvgrtp::frame::rtp_frame** out, 
    uint8_t payload_header_size, int flags)
{
    uvgrtp::buf_vec nalus;

    size_t size = 0;
    auto* frame = *out;

    for (size_t i = payload_header_size; i < frame->payload_len; 
        i += ntohs(*(uint16_t*)&frame->payload[i]) + sizeof(uint16_t)) {

        uint16_t packet_size = ntohs(*(uint16_t*)&frame->payload[i]);
        size += packet_size;

        if (size <= (*out)->payload_len) {
            nalus.push_back(std::make_pair(packet_size, &frame->payload[i] + sizeof(uint16_t)));
        }
        else {
            UVG_LOG_ERROR("The received aggregation packet claims to be larger than packet!");
            return RTP_GENERIC_ERROR;
        }
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
    uvgrtp::frame::rtp_frame* frame = *out;

    // aggregate, start, middle, end or single NAL
    uvgrtp::formats::FRAG_TYPE frag_type = get_fragment_type(frame); 
    
    if (frag_type == uvgrtp::formats::FRAG_TYPE::FT_AGGR) {

        // handle aggregate packets (packets with multiple NAL units in them)
        return handle_aggregation_packet(out, get_payload_header_size(), flags);
    }
    else if (frag_type == uvgrtp::formats::FRAG_TYPE::FT_NOT_FRAG) { // Single NAL unit

        // TODO: Check if previous dependencies have been sent forward

        // nothing special needs to be done, just possibly add start codes back
        prepend_start_code(flags, out);
        return RTP_PKT_READY;
    }
    else if (frag_type == uvgrtp::formats::FRAG_TYPE::FT_INVALID) {
        // something is wrong
        UVG_LOG_WARN("invalid frame received!");
        (void)uvgrtp::frame::dealloc_frame(*out);
        *out = nullptr;
        return RTP_GENERIC_ERROR;
    }

    // We have received a fragment. Rest of the function deals with fragmented frames

    // Fragment timestamp, all fragments of the same frame have the same timestamp
    uint32_t fragment_ts = frame->header.timestamp;

    // Fragment sequence number, determines the order of the fragments within frame
    uint16_t fragment_seq = frame->header.seq;      

    uvgrtp::formats::NAL_TYPE nal_type = get_nal_type(frame); // Intra, inter or some other type of frame
    
    // Initialize new frame if this is the first packet with this timestamp
    if (frames_.find(fragment_ts) == frames_.end()) {

        // Make sure we haven't discarded the frame corresponding to the fragment timestamp before 
        if (dropped_.find(fragment_ts) != dropped_.end()) {
            UVG_LOG_DEBUG("Fragment belonging to a dropped frame was received! Timestamp: %lu",
                fragment_ts);
            return RTP_GENERIC_ERROR;
        }

        initialize_new_fragmented_frame(fragment_ts, nal_type);
    }
    else if (frames_[fragment_ts].received_packet_seqs.find(fragment_seq) !=
        frames_[fragment_ts].received_packet_seqs.end()) {

        // we have already received this seq
        UVG_LOG_DEBUG("Detected duplicate fragment, dropping! Seq: %u", fragment_seq);
        (void)uvgrtp::frame::dealloc_frame(frame); // free fragment memory
        *out = nullptr;
        return RTP_GENERIC_ERROR;
    }

    const uint8_t sizeof_fu_headers = (uint8_t)get_payload_header_size() + 
                                               get_fu_header_size();

    if (frames_[fragment_ts].nal_type != nal_type)
    {
        UVG_LOG_ERROR("The fragment has different NAL type fragments before!");
        return RTP_GENERIC_ERROR;
    }

    // keep track of fragments belonging to this frame in case we need to delete them
    frames_[fragment_ts].received_packet_seqs.insert(fragment_seq);
    frames_[fragment_ts].total_size += (frame->payload_len - sizeof_fu_headers);

    if (fragments_[fragment_seq] != nullptr)
    {
        UVG_LOG_WARN("Found an existing fragment with same sequence number %u! Fragment ts: %lu, current ts: %lu",
            fragment_seq, fragments_[fragment_seq]->header.timestamp, fragment_ts);

        free_fragment(fragment_seq);
    }

    // save the fragment for later reconstruction
    fragments_[fragment_seq] = frame;

    // if this is first or last, save it to help with reconstruction
    if (frag_type == uvgrtp::formats::FRAG_TYPE::FT_START) {
        frames_[fragment_ts].s_seq = fragment_seq; 
        frames_[fragment_ts].start_received = true;
    }
    else if (frag_type == uvgrtp::formats::FRAG_TYPE::FT_END) {
        frames_[fragment_ts].e_seq = fragment_seq;
        frames_[fragment_ts].end_received = true;
    }

    // have the first and last fragment arrived so we can possibly start reconstructing the frame?
    if (frames_[fragment_ts].start_received && frames_[fragment_ts].end_received) {
        size_t received = calculate_expected_fus(fragment_ts);

        // have we received every fragment and can the frame can be reconstructed?
        if (received == frames_[fragment_ts].received_packet_seqs.size()) {

            bool enable_reference_discarding = (flags & RCE_H26X_DEPENDENCY_ENFORCEMENT);
            // here we discard inter frames if their references were not received correctly
            if (discard_until_key_frame_ && enable_reference_discarding) {
                if (nal_type == uvgrtp::formats::NAL_TYPE::NT_INTER) {
                    UVG_LOG_WARN("Dropping h26x frame because of missing reference. Timestamp: %lu. Seq: %u - %u",
                        fragment_ts, frames_[fragment_ts].s_seq, frames_[fragment_ts].e_seq);

                    drop_frame(fragment_ts);
                    return RTP_GENERIC_ERROR;
                }
                else if (nal_type == uvgrtp::formats::NAL_TYPE::NT_INTRA) {

                    // we don't have to discard anymore
                    UVG_LOG_INFO("Found a key frame at ts %lu", fragment_ts);
                    discard_until_key_frame_ = false;
                }
            }

            return reconstruction(out, flags, fragment_ts, sizeof_fu_headers);
        }
    }

    // make sure uvgRTP does not reserve increasing amounts of memory because some frames are not completed
    garbage_collect_lost_frames(rtp_ctx_->get_pkt_max_delay());
    return RTP_OK; // no frame was completed, but everything went ok for this fragment
}

void uvgrtp::formats::h26x::garbage_collect_lost_frames(size_t timout)
{
    if (uvgrtp::clock::hrc::diff_now(last_garbage_collection_) >= GARBAGE_COLLECTION_INTERVAL_MS) {
        uint32_t total_cleaned = 0;
        std::vector<uint32_t> to_remove;

        // first find all frames that have been waiting for too long
        for (auto& gc_frame : frames_) {
            if (uvgrtp::clock::hrc::diff_now(gc_frame.second.sframe_time) > timout) {
                UVG_LOG_WARN("Found an old frame that has not been completed");
                to_remove.push_back(gc_frame.first);
            }
        }

        // remove old frames
        for (auto& old_frame : to_remove) {

            total_cleaned += drop_frame(old_frame);
        }

        if (total_cleaned > 0) {
            UVG_LOG_INFO("Garbage collection cleaned %d bytes!", total_cleaned);
        }

        last_garbage_collection_ = uvgrtp::clock::hrc::now();
    }
}

void uvgrtp::formats::h26x::initialize_new_fragmented_frame(uint32_t ts, NAL_TYPE nal_type)
{
    frames_[ts].nal_type = nal_type;
    frames_[ts].s_seq = 0;
    frames_[ts].start_received = false;
    frames_[ts].e_seq = 0;
    frames_[ts].end_received = false;

    frames_[ts].sframe_time = uvgrtp::clock::hrc::now();
    frames_[ts].total_size = 0;
}

size_t uvgrtp::formats::h26x::calculate_expected_fus(uint32_t ts)
{
    size_t expected = 0;
    size_t s_seq = frames_[ts].s_seq;
    size_t e_seq = frames_[ts].e_seq;

    if (frames_[ts].start_received && frames_[ts].end_received)
    {
        if (s_seq > e_seq) {
            expected = (UINT16_MAX - s_seq) + e_seq + 2;
        }
        else {
            expected = e_seq - s_seq + 1;
        }
    }

    return expected;
}

void uvgrtp::formats::h26x::free_fragment(uint16_t sequence_number)
{
    if (fragments_[sequence_number] == nullptr)
    {
        UVG_LOG_ERROR("Tried to free an already freed fragment with seq: %u", sequence_number);
        return;
    }

    (void)uvgrtp::frame::dealloc_frame(fragments_[sequence_number]); // free fragment memory
    fragments_[sequence_number] = nullptr;
}

void uvgrtp::formats::h26x::scl(uint8_t* data, size_t data_len, size_t packet_size, 
    std::vector<nal_info>& nals, bool& can_be_aggregated)
{
    uint8_t start_len = 0;
    ssize_t offset = find_h26x_start_code(data, data_len, 0, start_len);

    packet_size -= get_payload_header_size(); // aggregate packet has a payload header

    while (offset > -1) {
        nal_info nal;
        nal.offset = size_t(offset);
        nal.prefix_len = start_len;
        nal.size = 0; // set after all NALs have been found
        nal.aggregate = false; // determined with size calculations


        nals.push_back(nal);
        offset = find_h26x_start_code(data, data_len, offset, start_len);
    }

    size_t aggregate_size = 0;
    int aggregatable_packets = 0;

    // calculate the sizes of NAL units
    for (size_t i = 0; i < nals.size(); ++i)
    {
        if (nals.size() > i + 1)
        {
            // take the difference of next NAL unit location and current one, 
            // minus size of start code prefix of next NAL unit
            nals.at(i).size = nals[i + 1].offset - nals[i].offset - nals[i + 1].prefix_len;
        }
        else
        {
            // last NAL unit, the length is offset to end
            nals.at(i).size = data_len - nals[i].offset;
        }

        // each NAL unit added to aggregate packet needs the size added which has to be taken into account
        // when calculating the aggregate packet 
        // (NOTE: This is not enough for MTAP in h264, but I doubt uvgRTP will support it)
        if (aggregate_size + nals.at(i).size + sizeof(uint16_t) <= packet_size)
        {
            aggregate_size += nals.at(i).size + sizeof(uint16_t);
            nals.at(i).aggregate = true;
            ++aggregatable_packets;
        }
    }

    can_be_aggregated = (aggregatable_packets >= 2);
}

rtp_error_t uvgrtp::formats::h26x::reconstruction(uvgrtp::frame::rtp_frame** out, 
    int flags, uint32_t frame_timestamp, const uint8_t sizeof_fu_headers)
{
    uvgrtp::frame::rtp_frame* frame = *out;

    // Reconstruction of frame from fragments
    size_t fptr = 0;

    // allocating the frame with start code ready saves a copy operation for the frame
    uvgrtp::frame::rtp_frame* complete = allocate_rtp_frame_with_startcode((flags & RCE_H26X_PREPEND_SC),
        frame->header, get_nal_header_size() + frames_[frame_timestamp].total_size, fptr);

    // construct the NAL header from fragment header of current fragment
    get_nal_header_from_fu_headers(fptr, frame->payload, complete->payload); // NAL header
    fptr += get_nal_header_size();

    uint16_t next_from_last = frames_.at(frame_timestamp).e_seq + 1;
    for (uint16_t i = frames_.at(frame_timestamp).s_seq; i != next_from_last; ++i)
    {
        if (fragments_[i] == nullptr)
        {
            UVG_LOG_ERROR("Missing fragment in reconstruction. Seq range: %u - %u. Missing seq %u",
                frames_.at(frame_timestamp).s_seq, frames_.at(frame_timestamp).e_seq, i);
            return RTP_GENERIC_ERROR;
        }

        // copy everything expect fu headers (which repeat for every fu)
        std::memcpy(
            &complete->payload[fptr],
            &fragments_[i]->payload[sizeof_fu_headers],
            fragments_[i]->payload_len - sizeof_fu_headers
        );
        fptr += fragments_[i]->payload_len - sizeof_fu_headers;
        free_fragment(i);
    }

    *out = complete;      // save result to output
    frames_.erase(frame_timestamp);  // erase data structures for this frame
    return RTP_PKT_READY; // indicate that we have a frame ready
}
