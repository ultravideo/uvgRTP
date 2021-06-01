#include "formats/h264.hh"

#include "queue.hh"
#include "debug.hh"

#include <cstdint>
#include <cstring>
#include <iostream>
#include <map>
#include <unordered_set>

#define RTP_FRAME_MAX_DELAY          100
#define INVALID_SEQ           0x13371338
#define INVALID_TS            0xffffffff

#define RTP_HDR_SIZE  12
#define NAL_HDR_SIZE   1

enum FRAG_TYPES {
    FT_INVALID   = -2, /* invalid combination of S and E bits */
    FT_NOT_FRAG  = -1, /* frame doesn't contain HEVC fragment */
    FT_START     =  1, /* frame contains a fragment with S bit set */
    FT_MIDDLE    =  2, /* frame is fragment but not S or E fragment */
    FT_END       =  3, /* frame contains a fragment with E bit set */
    FT_STAP_A    =  4  /* Single-Time Aggregation Packet, Type A */
};

enum NAL_TYPES {
    NT_INTRA = 0x00,
    NT_INTER = 0x01,
    NT_OTHER = 0xff
};

static int __get_frag(uvgrtp::frame::rtp_frame *frame)
{
    bool first_frag = frame->payload[1] & 0x80;
    bool last_frag  = frame->payload[1] & 0x40;

    if ((frame->payload[0] & 0x1f) == uvgrtp::formats::H264_PKT_AGGR)
        return FT_STAP_A;

    if ((frame->payload[0] & 0x1f) != uvgrtp::formats::H264_PKT_FRAG)
        return FT_NOT_FRAG;

    if (first_frag && last_frag)
        return FT_INVALID;

    if (first_frag)
        return FT_START;

    if (last_frag)
        return FT_END;

    return FT_MIDDLE;
}

static rtp_error_t __handle_stap_a(uvgrtp::formats::h264_frame_info_t *finfo, uvgrtp::frame::rtp_frame **out)
{
    uvgrtp::buf_vec nalus;

    size_t size  = 0;
    auto  *frame = *out;

    for (size_t i = uvgrtp::frame::HEADER_SIZE_H264_FU; i < frame->payload_len; ) {
        nalus.push_back(
            std::make_pair(
                ntohs(*(uint16_t *)&frame->payload[i]),
                &frame->payload[i] + sizeof(uint16_t)
            )
        );

        size += ntohs(*(uint16_t *)&frame->payload[i]);
        i    += ntohs(*(uint16_t *)&frame->payload[i]) + sizeof(uint16_t);
    }

    for (size_t i = 0; i < nalus.size(); ++i) {
        auto retframe = uvgrtp::frame::alloc_rtp_frame(nalus[i].first);

        std::memcpy(
            retframe->payload,
            nalus[i].second,
            nalus[i].first
        );

        finfo->queued.push_back(retframe);
    }

    return RTP_MULTIPLE_PKTS_READY;
}

static inline uint8_t __get_nal(uvgrtp::frame::rtp_frame *frame)
{
    switch (frame->payload[1] & 0x1f) {
        case 19: return NT_INTRA;
        case 1:  return NT_INTER;
        default: break;
    }

    return NT_OTHER;
}

static inline bool __frame_late(uvgrtp::formats::h264_info_t& hinfo)
{
    return (uvgrtp::clock::hrc::diff_now(hinfo.sframe_time) >= RTP_FRAME_MAX_DELAY);
}

static void __drop_frame(uvgrtp::formats::h264_frame_info_t *finfo, uint32_t ts)
{
    uint16_t s_seq = finfo->frames.at(ts).s_seq;
    uint16_t e_seq = finfo->frames.at(ts).e_seq;

    LOG_INFO("Dropping frame %u, %u - %u", ts, s_seq, e_seq);

    for (auto& fragment : finfo->frames.at(ts).fragments)
        (void)uvgrtp::frame::dealloc_frame(fragment.second);

    finfo->frames.erase(ts);
}

rtp_error_t uvgrtp::formats::h264::packet_handler(void *arg, int flags, uvgrtp::frame::rtp_frame **out)
{
    uvgrtp::frame::rtp_frame *frame;
    bool enable_idelay = !(flags & RCE_NO_H26X_INTRA_DELAY);
    auto finfo = (uvgrtp::formats::h264_frame_info_t *)arg;

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

    const size_t AVC_HDR_SIZE =
        uvgrtp::frame::HEADER_SIZE_H264_NAL +
        uvgrtp::frame::HEADER_SIZE_H264_FU;

    frame = *out;

    uint32_t c_ts    = frame->header.timestamp;
    uint32_t c_seq   = frame->header.seq;
    int frag_type    = __get_frag(frame);
    uint8_t nal_type = __get_nal(frame);

    if (frag_type == FT_STAP_A)
        return __handle_stap_a(finfo, out);

    if (frag_type == FT_NOT_FRAG)
        return RTP_PKT_READY;

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

        finfo->frames[c_ts].sframe_time   = uvgrtp::clock::hrc::now();
        finfo->frames[c_ts].total_size    = frame->payload_len - AVC_HDR_SIZE;
        finfo->frames[c_ts].pkts_received = 1;

        finfo->frames[c_ts].fragments[c_seq] = frame;
        return RTP_OK;
    }

    finfo->frames[c_ts].pkts_received += 1;
    finfo->frames[c_ts].total_size    += (frame->payload_len - AVC_HDR_SIZE);

    if (frag_type == FT_START) {
        finfo->frames[c_ts].s_seq = c_seq;
        finfo->frames[c_ts].fragments[c_seq] = frame;

        for (auto& fragment : finfo->frames[c_ts].temporary) {
            uint16_t fseq = fragment->header.seq;
            uint32_t seq  = (c_seq > fseq) ? 0x10000 + fseq : fseq;

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
        } else {
            /* position for the fragment cannot be calculated so move the fragment to a temporary storage */
            finfo->frames[c_ts].temporary.push_back(frame);
        }
    }

    if (finfo->frames[c_ts].s_seq != INVALID_SEQ && finfo->frames[c_ts].e_seq != INVALID_SEQ) {
        size_t received = 0;
        size_t fptr     = 0;
        size_t s_seq    = finfo->frames[c_ts].s_seq;
        size_t e_seq    = finfo->frames[c_ts].e_seq;

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

            uvgrtp::frame::rtp_frame *complete = uvgrtp::frame::alloc_rtp_frame();

            complete->payload_len = finfo->frames[c_ts].total_size + uvgrtp::frame::HEADER_SIZE_H264_NAL;
            complete->payload     = new uint8_t[complete->payload_len];

            std::memcpy(&complete->header,  &(*out)->header, RTP_HDR_SIZE);
            complete->payload[0] = (frame->payload[0] & 0xe0) | (frame->payload[1] & 0x1f);

            fptr += uvgrtp::frame::HEADER_SIZE_H264_NAL;

            for (auto& fragment : finfo->frames.at(c_ts).fragments) {
                std::memcpy(
                    &complete->payload[fptr],
                    &fragment.second->payload[AVC_HDR_SIZE],
                    fragment.second->payload_len - AVC_HDR_SIZE
                );
                fptr += fragment.second->payload_len - AVC_HDR_SIZE;
                (void)uvgrtp::frame::dealloc_frame(fragment.second);
            }

            if (nal_type == NT_INTRA)
                intra = INVALID_TS;

            *out = complete;
            finfo->frames.erase(c_ts);
            return RTP_PKT_READY;
        }
    }

    if (__frame_late(finfo->frames.at(c_ts))) {
        if (nal_type != NT_INTRA || (nal_type == NT_INTRA && !enable_idelay)) {
            __drop_frame(finfo, c_ts);
            finfo->dropped.insert(c_ts);
        }
    }

    return RTP_OK;
}
