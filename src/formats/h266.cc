#include "h266.hh"

#include "../rtp.hh"
#include "../queue.hh"
#include "frame.hh"
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

#define NAL_HDR_SIZE   2

static int __get_frag(uvgrtp::frame::rtp_frame* frame)
{
    bool first_frag = frame->payload[2] & 0x80;
    bool last_frag = frame->payload[2] & 0x40;

    if ((frame->payload[1] >> 3) != uvgrtp::formats::H266_PKT_FRAG)
        return uvgrtp::formats::FT_NOT_FRAG;

    if (first_frag && last_frag)
        return uvgrtp::formats::FT_INVALID;

    if (first_frag)
        return uvgrtp::formats::FT_START;

    if (last_frag)
        return uvgrtp::formats::FT_END;

    return uvgrtp::formats::FT_MIDDLE;
}

static inline uint8_t __get_nal(uvgrtp::frame::rtp_frame* frame)
{
    switch (frame->payload[2] & 0x3f) {
    case 19: return uvgrtp::formats::NT_INTRA;
    case 1:  return uvgrtp::formats::NT_INTER;
    default: break;
    }

    return uvgrtp::formats::NT_OTHER;
}

static inline bool __frame_late(uvgrtp::formats::h266_info_t& hinfo, size_t max_delay)
{
    return (uvgrtp::clock::hrc::diff_now(hinfo.sframe_time) >= max_delay);
}

static void __drop_frame(uvgrtp::formats::h266_frame_info_t* finfo, uint32_t ts)
{
    uint16_t s_seq = finfo->frames.at(ts).s_seq;
    uint16_t e_seq = finfo->frames.at(ts).e_seq;

    LOG_INFO("Dropping frame %u, %u - %u", ts, s_seq, e_seq);

    for (auto& fragment : finfo->frames.at(ts).fragments)
        (void)uvgrtp::frame::dealloc_frame(fragment.second);

    finfo->frames.erase(ts);
}


uvgrtp::formats::h266::h266(uvgrtp::socket* socket, uvgrtp::rtp* rtp, int flags) :
    h26x(socket, rtp, flags), finfo_{}
{
    finfo_.rtp_ctx = rtp;
}

uvgrtp::formats::h266::~h266()
{
}

uint8_t uvgrtp::formats::h266::get_nal_type(uint8_t* data)
{
    return (data[1] >> 3) & 0x1f;
}

uvgrtp::formats::h266_frame_info_t *uvgrtp::formats::h266::get_h266_frame_info()
{
    return &finfo_;
}

rtp_error_t uvgrtp::formats::h266::handle_small_packet(uint8_t* data, size_t data_len, bool more)
{
    rtp_error_t ret = RTP_OK;

    if ((ret = fqueue_->enqueue_message(data, data_len)) != RTP_OK) {
        LOG_ERROR("enqeueu failed for small packet");
        return ret;
    }

    if (more)
        return RTP_NOT_READY;
    return fqueue_->flush_queue();
}

rtp_error_t uvgrtp::formats::h266::construct_format_header_divide_fus(uint8_t* data, size_t& data_left,
    size_t& data_pos, size_t payload_size, uvgrtp::buf_vec& buffers)
{
    auto headers = (uvgrtp::formats::h266_headers*)fqueue_->get_media_headers();

    headers->nal_header[0] = data[0];
    headers->nal_header[1] = (29 << 3) | (data[1] & 0x7);

    initialize_fu_headers(get_nal_type(data), headers->fu_headers);

    buffers.push_back(std::make_pair(sizeof(headers->nal_header), headers->nal_header));
    buffers.push_back(std::make_pair(sizeof(uint8_t), &headers->fu_headers[0]));
    buffers.push_back(std::make_pair(payload_size, nullptr));

    data_pos = uvgrtp::frame::HEADER_SIZE_H266_NAL;
    data_left -= uvgrtp::frame::HEADER_SIZE_H266_NAL;

    return divide_frame_to_fus(data, data_left, data_pos, payload_size, buffers, headers->fu_headers);
}

rtp_error_t uvgrtp::formats::h266::packet_handler(void* arg, int flags, uvgrtp::frame::rtp_frame** out)
{
    uvgrtp::frame::rtp_frame* frame;
    bool enable_idelay = !(flags & RCE_NO_H26X_INTRA_DELAY);
    auto finfo = (uvgrtp::formats::h266_frame_info_t*)arg;

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

    const size_t H266_HDR_SIZE =
        uvgrtp::frame::HEADER_SIZE_H266_NAL +
        uvgrtp::frame::HEADER_SIZE_H266_FU;

    frame = *out;

    uint32_t c_ts = frame->header.timestamp;
    uint32_t c_seq = frame->header.seq;
    int frag_type = __get_frag(frame);
    uint8_t nal_type = __get_nal(frame);

    /* if (frag_type == FT_AGGR) */
    /*     return __handle_ap(finfo, out); */

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
    if (finfo->frames.find(c_ts) == finfo->frames.end()) {

        /* make sure we haven't discarded the frame "c_ts" before */
        if (finfo->dropped.find(c_ts) != finfo->dropped.end()) {
            LOG_WARN("packet belonging to a dropped frame was received!");
            return RTP_GENERIC_ERROR;
        }

        /* drop old intra if a new one is received */
        if (nal_type == NT_INTRA) {
            if (intra != INVALID_TS && enable_idelay) {
                __drop_frame(finfo, intra);
                finfo->dropped.insert(intra);
            }
            intra = c_ts;
        }

        finfo->frames[c_ts].s_seq = INVALID_SEQ;
        finfo->frames[c_ts].e_seq = INVALID_SEQ;

        if (frag_type == FT_START) finfo->frames[c_ts].s_seq = c_seq;
        if (frag_type == FT_END)   finfo->frames[c_ts].e_seq = c_seq;

        finfo->frames[c_ts].sframe_time = uvgrtp::clock::hrc::now();
        finfo->frames[c_ts].total_size = frame->payload_len - H266_HDR_SIZE;
        finfo->frames[c_ts].pkts_received = 1;

        finfo->frames[c_ts].fragments[c_seq] = frame;
        return RTP_OK;
    }

    finfo->frames[c_ts].pkts_received += 1;
    finfo->frames[c_ts].total_size += (frame->payload_len - H266_HDR_SIZE);

    if (frag_type == FT_START) {
        finfo->frames[c_ts].s_seq = c_seq;
        finfo->frames[c_ts].fragments[c_seq] = frame;

        for (auto& fragment : finfo->frames[c_ts].temporary) {
            uint16_t fseq = fragment->header.seq;
            uint32_t seq = (c_seq > fseq) ? 0x10000 + fseq : fseq;

            finfo->frames[c_ts].fragments[seq] = fragment;
        }
        finfo->frames[c_ts].temporary.clear();
    }

    if (frag_type == FT_END)
        finfo->frames[c_ts].e_seq = c_seq;

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
        if (finfo->frames[c_ts].s_seq != INVALID_SEQ) {
            /* overflow has occurred, adjust the sequence number of current
             * fragment so it appears in order with other fragments of the frame
             *
             * Note: if the frame is huge (~94 MB), this will not work but it's not a realistic scenario */
            finfo->frames[c_ts].fragments[((finfo->frames[c_ts].s_seq > c_seq) ? 0x10000 + c_seq : c_seq)] = frame;
        }
        else {
            /* position for the fragment cannot be calculated so move the fragment to a temporary storage */
            finfo->frames[c_ts].temporary.push_back(frame);
        }
    }

    if (finfo->frames[c_ts].s_seq != INVALID_SEQ && finfo->frames[c_ts].e_seq != INVALID_SEQ) {
        size_t received = 0;
        size_t fptr = 0;
        size_t s_seq = finfo->frames[c_ts].s_seq;
        size_t e_seq = finfo->frames[c_ts].e_seq;

        if (s_seq > e_seq)
            received = 0xffff - s_seq + e_seq + 2;
        else
            received = e_seq - s_seq + 1;

        /* we've received every fragment and the frame can be reconstructed */
        if (received == finfo->frames[c_ts].pkts_received) {

            /* intra is still in progress, do not return the inter */
            if (nal_type == NT_INTER && intra != INVALID_TS && enable_idelay) {
                __drop_frame(finfo, c_ts);
                finfo->dropped.insert(c_ts);
                return RTP_OK;
            }

            uint8_t nal_header[2] = {
                (uint8_t)((frame->payload[0] & 0x81) | ((frame->payload[2] & 0x3f) << 1)),
                (uint8_t)frame->payload[1]
            };

            uvgrtp::frame::rtp_frame* complete = uvgrtp::frame::alloc_rtp_frame();

            complete->payload_len =
                finfo->frames[c_ts].total_size
                + uvgrtp::frame::HEADER_SIZE_H266_NAL +
                +((flags & RCE_H26X_PREPEND_SC) ? 4 : 0);

            complete->payload = new uint8_t[complete->payload_len];

            if (flags & RCE_H26X_PREPEND_SC) {
                complete->payload[0] = 0;
                complete->payload[1] = 0;
                complete->payload[2] = 0;
                complete->payload[3] = 1;
                fptr += 4;
            }

            std::memcpy(&complete->header, &(*out)->header, RTP_HDR_SIZE);
            std::memcpy(&complete->payload[fptr], nal_header, NAL_HDR_SIZE);

            fptr += uvgrtp::frame::HEADER_SIZE_H266_NAL;

            for (auto& fragment : finfo->frames.at(c_ts).fragments) {
                std::memcpy(
                    &complete->payload[fptr],
                    &fragment.second->payload[H266_HDR_SIZE],
                    fragment.second->payload_len - H266_HDR_SIZE
                );
                fptr += fragment.second->payload_len - H266_HDR_SIZE;
                (void)uvgrtp::frame::dealloc_frame(fragment.second);
            }

            if (nal_type == NT_INTRA)
                intra = INVALID_TS;

            *out = complete;
            finfo->frames.erase(c_ts);
            return RTP_PKT_READY;
        }
    }

    if (__frame_late(finfo->frames.at(c_ts), finfo->rtp_ctx->get_pkt_max_delay())) {
        if (nal_type != NT_INTRA || (nal_type == NT_INTRA && !enable_idelay)) {
            __drop_frame(finfo, c_ts);
            finfo->dropped.insert(c_ts);
        }
    }

    return RTP_OK;
}
