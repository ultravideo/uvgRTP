#include <cstdint>
#include <cstring>
#include <iostream>
#include <queue>
#include <unordered_map>

#include "conn.hh"
#include "debug.hh"
#include "reader.hh"
#include "queue.hh"
#include "send.hh"
#include "writer.hh"

#define INVALID_SEQ           0x13371338
#define INVALID_TS            0xffffffff

#define MAX_DATAGRAMS                  5
#define RTP_FRAME_MAX_DELAY           50

#define RTP_HDR_SIZE                  12
#define NAL_HDR_SIZE                   2
#define FU_HDR_SIZE                    1

#define INACTIVE(ts)    (buff.inactive[(ts)])

const int DEFAULT_ALLOC_SIZE = 200 * MAX_PAYLOAD;
const int REALLOC_SIZE       = 50  * MAX_PAYLOAD;  /* TODO: realloc size should be calculated dynamically */

enum FRAG_TYPES {
    /* invalid combination of S and E bits */
    FT_INVALID   = -2,

    /* frame doesn't contain HEVC fragment */
    FT_NOT_FRAG  = -1,

    /* frame contains a fragment with S bit set */
    FT_START     =  1,

    /* frame is fragment but not S or E fragment */
    FT_MIDDLE    =  2,

    /* frame contains a fragment with E bit set */
    FT_END       =  3,
};

/* Buffer contains three bytes: 2 byte NAL header and 1 byte FU header */
static int __get_frame_type(uint8_t *buffer)
{
    bool first_frag = buffer[2] & 0x80;
    bool last_frag  = buffer[2] & 0x40;

    if ((buffer[0] >> 1) != 49)
        return FT_NOT_FRAG;

    if (first_frag && last_frag)
        return FT_INVALID;

    if (first_frag)
        return FT_START;

    if (last_frag)
        return FT_END;

    return FT_MIDDLE;
}

static inline uint32_t __get_next_ts(std::queue<uint32_t>& ts)
{
    uint32_t n_ts = ts.front();
    ts.pop();

    return n_ts;
}

/* Calculate the absolute offset within frame at "index"
 *
 * For empty frames the start offet ("start") might be 0 so we need
 * to take that into consideration when calculating the offset */
static inline size_t __calculate_offset(size_t start, size_t index)
{
    size_t off = index * MAX_PAYLOAD;

    if (start > NAL_HDR_SIZE)
        off = start - MAX_DATAGRAMS * MAX_PAYLOAD + off;
    else
        off += NAL_HDR_SIZE;

    return off;
}

/* TODO: tämä koodi pitää kommentoida todella hyvin! */

enum RELOC_TYPES {

    /* Fragment is already in frame, 
     * most likely in correct place too (verification needed) */
    RT_PAYLOAD   = 0,

    /* Fragment has been copied to probation zone and it should be
     * relocated to frame */
    RT_PROBATION = 1,

    /* Probation zone has been disabled or it has run out of space and
     * the fragment has been copied to separate RTP frame and stored 
     * to "probation" vector */
    RT_FRAME     = 2
};

struct reloc_info {
    size_t   c_off; /* current offset in the frame */
    size_t   d_off; /* destination offset */
    void     *ptr;  /* pointer to memory */
    int reloc_type; /* relocation type (see RELOC_TYPES) */
};

struct inactive_info {
    size_t pkts_received;
    size_t total_size;
    size_t received_size;

    kvz_rtp::frame::rtp_frame *frame;

    size_t next_off;

    /* TODO: relocs? */
    std::map<uint16_t, struct reloc_info> rinfo;

    uint32_t s_seq;
    uint32_t e_seq;

    kvz_rtp::clock::hrc::hrc_t start; /* clock reading when the first fragment is received */
};

/* TODO: muuta recv_buffer sisältämään kaiken tämän turhan datan sijaan
 * vain inactive_infoja (future: frame_info) ja frame_info UNORDERED_MAPIN lisäksi
 * säilytä tieto aktiivisen framen timestampista! */

struct recv_buffer {
    /* One large block of contiguos memory 
     * This may be much larger than the actual frame (large internal fragmentation)
     * but the algorithm tries to learn from TODO 
     *
     * Henceforth this is the "receiver frame" */
    kvz_rtp::frame::rtp_frame *frame;

    /* TODO:  */
    kvz_rtp::frame::rtp_header rtp_headers[MAX_DATAGRAMS];
    uint8_t hevc_ext_buf[MAX_DATAGRAMS][NAL_HDR_SIZE + FU_HDR_SIZE];

    /* TODO:  */
    struct mmsghdr headers[MAX_DATAGRAMS];
    struct iovec iov[MAX_DATAGRAMS * 3];

    /* Because UDP packets don't come in order (but we read them as if they are in order)
     * we need to some relocations when all fragments have been received.
     *
     * Keep a separate buffer of the relocation information which holds the current position
     * and the correct position of the frame within the full frame */
    std::vector<std::pair<size_t, size_t>> reloc_needed;

    /* How many packets we've received */
    size_t pkts_received;

    /*  */
    size_t total_size;
    size_t received_size;

    /*  */
    size_t next_off;

    /* Timestamp of the active frame */
    uint32_t active_ts;

    /* Sequence number of previous frame's last fragment. 
     *
     * This number is used to check the placement of new fragments within the frame 
     * and calculate new position for the fragments if they're out of order */
    uint32_t prev_f_seq;

    std::unordered_map<uint32_t, bool> seqs;

    std::unordered_map<uint32_t, inactive_info> inactive;
    std::queue<uint32_t> tss; /* TODO: this is beyond ugly */

    kvz_rtp::clock::hrc::hrc_t start; /* clock reading when the first fragment is received */
    uint32_t s_seq;                    /* sequence number of the frame with s-bit */
    uint32_t e_seq;                    /* sequence number of the frame with e-bit */
};

struct frame_info {
    /* One big frame for all fragments, this is resized if all space is consumed */
    kvz_rtp::frame::rtp_frame *frame;

    size_t total_size;    /* allocated size */
    size_t received_size; /* used size */
    size_t pkts_received; /* # packets received (used to detect when all fragments have been received) */

    /* next fragment slot in the frame */
    size_t next_off;

    /* Fragments that require relocation within frame */
    std::map<uint16_t, struct reloc_info> rinfo;

    /* If probation zone is disabled or all its memory has been used
     * fragments that cannot be relocated are pushed here and when all 
     * fragments have been received, the fragments are copied from probation to the frame */
    /* TODO: käyät rinfoa tämän soktun sijaan */
    std::vector<kvz_rtp::frame::rtp_frame *> probation;

    /* Store all received sequence numbers here so we can detect duplicate packets */
    std::unordered_map<uint32_t, bool> seqs;

    /* start and end sequences of the frame (frames with S/E bit set) */
    uint32_t s_seq;
    uint32_t e_seq;

    /* clock reading when the first fragment is received */
    kvz_rtp::clock::hrc::hrc_t start;
};

struct frames {
    /* Global (and overwritable) buffers used for fragment receiving */
    kvz_rtp::frame::rtp_header rtp_headers[MAX_DATAGRAMS];
    uint8_t hevc_ext_buf[MAX_DATAGRAMS][NAL_HDR_SIZE + FU_HDR_SIZE];
    struct mmsghdr headers[MAX_DATAGRAMS];
    struct iovec iov[MAX_DATAGRAMS * 3];

    /* timestamp of the old (still active frame), used to index finfo */
    uint32_t active_ts;

    /* Frames are handled FIFO style so keep the timestamps in a queue */
    std::queue<uint32_t> tss;

    /* All active and inactive frames */
    std::unordered_map<uint32_t, struct frame_info> finfo;
};

rtp_error_t __hevc_receiver_optimistic(kvz_rtp::reader *reader)
{
    int socket    = reader->get_raw_socket();
    int pkts_read = 0;

    recv_buffer buff;
    buff.total_size    = 2 * DEFAULT_ALLOC_SIZE + NAL_HDR_SIZE;
    buff.frame         = kvz_rtp::frame::alloc_rtp_frame(buff.total_size);
    buff.next_off      = NAL_HDR_SIZE;
    buff.pkts_received = 0;

    buff.active_ts     = INVALID_TS;
    buff.prev_f_seq    = INVALID_SEQ;
    buff.s_seq         = INVALID_SEQ;
    buff.e_seq         = INVALID_SEQ;

    std::memset(buff.headers, 0, sizeof(buff.headers));

    uint64_t avg_us = 0, avg_us_total = 0;
    uint64_t avg_fs = 0, avg_fs_total = 0;

    while (reader->active()) {
        for (size_t i = 0, k = 0; i < MAX_DATAGRAMS; ++i, k += 3) {

            if (buff.next_off + MAX_DATAGRAMS * MAX_PAYLOAD > buff.total_size) {
                LOG_ERROR("Reallocate RTP frame from %u to %u!", buff.total_size, buff.total_size + REALLOC_SIZE);
                for (;;);
#if 0
                auto tmp_frame = kvz_rtp::frame::alloc_rtp_frame(buff.total_size + REALLOC_SIZE);

                std::memcpy(
                    tmp_frame->payload,
                    buff.frame->payload,
                    buff.total_size
                );

                (void)kvz_rtp::frame::dealloc_frame(buff.frame);
                buff.frame = tmp_frame;
                buff.total_size += REALLOC_SIZE;

                /* for (;;); */
#endif
            }

            buff.iov[k + 0].iov_base = &buff.rtp_headers[i];
            buff.iov[k + 0].iov_len  = sizeof(buff.rtp_headers[i]);

            buff.iov[k + 1].iov_base = &buff.hevc_ext_buf[i];
            buff.iov[k + 1].iov_len  = 3;

            buff.iov[k + 2].iov_base = buff.frame->payload + buff.next_off;
            buff.iov[k + 2].iov_len  = MAX_PAYLOAD;
            buff.next_off           += MAX_PAYLOAD;

            buff.headers[i].msg_hdr.msg_iov    = &buff.iov[k];
            buff.headers[i].msg_hdr.msg_iovlen = 3;
        }

        if ((pkts_read = recvmmsg(socket, buff.headers, MAX_DATAGRAMS, 0, nullptr)) < 0) {
            LOG_ERROR("recvmmsg() failed, %s", strerror(errno));
            continue;
        }

        uint32_t p_seq = INVALID_SEQ;

        struct shift_info {
            bool shift_needed;
            size_t shift_size;
            size_t shift_offset;
        } sinfo;

        sinfo.shift_needed = false;
        sinfo.shift_size   = 0;
        sinfo.shift_offset = 0;

        for (size_t i = 0; i < MAX_DATAGRAMS; ++i) {
            int type = __get_frame_type(buff.hevc_ext_buf[i]);

            buff.rtp_headers[i].timestamp = ntohl(buff.rtp_headers[i].timestamp);
            buff.rtp_headers[i].seq       = ntohs(buff.rtp_headers[i].seq);

            uint32_t c_ts  = buff.rtp_headers[i].timestamp;
            uint32_t c_seq = buff.rtp_headers[i].seq;

            /* Previous fragment was returned to user/moved to other frame and now 
             * it's considered garbage memory as far as active frame is concerned
             *
             * We must clean this "garbage" by shifting current fragment 
             * (and all subsequent fragments) so the full HEVC frame can be decoded
             * successfully
             *
             * There are two kinds of shifts: overwriting and appending shifts. 
             *
             * Overwriting shirts, as the name suggests, overwrite the previous content
             * so the shift offset is not updated between shifts. Non-fragments and fragments
             * that belong to other frames cause overwriting shifts. 
             *
             * Overwriting shifts also cause appending shifts because when a fragment is removed,
             * and a valid fragment is shifted on its place, this valid fragment must not be overwritten 
             * and all subsequent valid fragments must be appended.*/
            if (sinfo.shift_needed) {
                size_t c_off = __calculate_offset(buff.next_off, i);

                /* We don't need to shift non-fragments and invalid data
                 * because non-fragments will be copied to other frame from their currenct position
                 * in the frame and both non-fragments and invalid packets will be overwritten */
                if (type != FT_NOT_FRAG && type != FT_INVALID) {
                    std::memcpy(
                        buff.frame->payload + sinfo.shift_offset,
                        buff.frame->payload + c_off,
                        MAX_PAYLOAD
                    );
                }
            }

            if (type == FT_NOT_FRAG) {
                size_t len = buff.headers[i].msg_len - RTP_HDR_SIZE;
                auto frame = kvz_rtp::frame::alloc_rtp_frame(len);
                size_t off = __calculate_offset(buff.next_off, i);

                /* LOG_WARN("not fragment %u", len); */
                /* TODO: add good comment */
                if (!sinfo.shift_needed) {
                    sinfo.shift_needed = true;
                    sinfo.shift_offset = off;
                }

                std::memcpy(frame->payload + 0, &buff.hevc_ext_buf[i], 3);
                std::memcpy(frame->payload + 3, buff.frame->payload + off, len - 3);

                buff.prev_f_seq = c_seq;

                reader->return_frame(frame);
                continue;
            }

            if (type == FT_INVALID) {
                /* TODO: update shift info */
                LOG_WARN("Invalid frame received!");
                for (;;);
                continue;
            }

            /* this is for first frame only TODO: ei pidä paikkaansa enää */
            if (buff.active_ts == INVALID_TS)
                buff.active_ts = c_ts;

            if (buff.active_ts == c_ts) {
                buff.pkts_received++;
                buff.received_size += (buff.headers[i].msg_len - RTP_HDR_SIZE - NAL_HDR_SIZE - FU_HDR_SIZE);

                if (sinfo.shift_needed) {
                    /* we have shifted the memory to correct place above but we need to
                     * update the offset. Because this fragment belongs to this frame, 
                     * next shift (if there are fragments left) must be appending instead of overwriting shift */
                    /* sinfo.shift_offset += MAX_PAYLOAD; */

                    /* TODO: käytä tätä kokoa, sillä saadaan framesta pienempi */
                    sinfo.shift_offset += buff.headers[i].msg_len - RTP_HDR_SIZE - NAL_HDR_SIZE - FU_HDR_SIZE;
                }
            } else {
                /* seuraavan framen fragmentti tuli keskellä vielä keskeneräistä frame, 
                 * joten kaikkia fragmentteja jotka seuraavat tätä fragmenttia joudutaan shiftaamaan */

                /* TODO: selitä tämä koko else höskä */

                if (i + 1 < MAX_DATAGRAMS) {
                    sinfo.shift_needed = true;
                    sinfo.shift_offset = buff.next_off - MAX_DATAGRAMS * MAX_PAYLOAD + i * MAX_PAYLOAD;
                    sinfo.shift_size   = buff.headers[i].msg_len - RTP_HDR_SIZE - NAL_HDR_SIZE - FU_HDR_SIZE;
                }

                /* TODO: selitä */
                size_t len = buff.headers[i].msg_len - RTP_HDR_SIZE - NAL_HDR_SIZE - FU_HDR_SIZE;
                size_t off = __calculate_offset(buff.next_off, i);

                if (buff.inactive.find(c_ts) == buff.inactive.end()) {
                    buff.tss.push(c_ts);

                    INACTIVE(c_ts).pkts_received = 0;
                    INACTIVE(c_ts).frame         = kvz_rtp::frame::alloc_rtp_frame(DEFAULT_ALLOC_SIZE + NAL_HDR_SIZE);
                    INACTIVE(c_ts).total_size    = DEFAULT_ALLOC_SIZE + NAL_HDR_SIZE;
                    INACTIVE(c_ts).next_off      = MAX_PAYLOAD + NAL_HDR_SIZE;
                    INACTIVE(c_ts).received_size = MAX_PAYLOAD + NAL_HDR_SIZE;

                    if (type == FT_START) {
                        INACTIVE(c_ts).s_seq = c_seq;
                        INACTIVE(c_ts).start = kvz_rtp::clock::hrc::now();

                        fprintf(stderr, "start %u: copy %u bytes from %u to %u\n", c_ts, MAX_PAYLOAD, off, NAL_HDR_SIZE);
                        std::memcpy(
                            INACTIVE(c_ts).frame->payload + NAL_HDR_SIZE,
                            buff.frame->payload + off,
                            MAX_PAYLOAD
                        );
                    } else {
                        LOG_WARN("frame must be copied to probation area");
                    }

                /* This is not the first fragment of an inactive frame, copy it to correct frame
                 * or to probation area if its place cannot be determined (s_seq must be known) */
                } else {
                    if (buff.inactive[c_ts].s_seq != INVALID_SEQ) {
                        fprintf(stderr, "not start %u: copy %u bytes from %u to %u\n", c_ts, MAX_PAYLOAD, off, INACTIVE(c_ts).next_off);
                        std::memcpy(
                            INACTIVE(c_ts).frame->payload + INACTIVE(c_ts).next_off,
                            buff.frame->payload + off,
                            MAX_PAYLOAD
                        );

                        INACTIVE(c_ts).rinfo.insert(
                            std::make_pair<uint16_t, struct reloc_info>(
                                (uint16_t)c_seq,
                                { INACTIVE(c_ts).next_off, 0 }
                            )
                        );

                        INACTIVE(c_ts).next_off += MAX_PAYLOAD;
                    } else {
                        LOG_WARN("frame must be copied to probation area");
#ifdef __RTP_USE_PROBATION_ZONE__
                        if (buff.frame->probation_off != buff.frame->probation_len) {
                            std::memcpy(
                                buff.frame->probation + buff.frame->probation_off,
                                buff.frame->payload + off,
                                MAX_PAYLOAD
                            );

                            buff.frame->probation_off += MAX_PAYLOAD;
                        } else {
                            auto tmp_frame = kvz_rtp::frame::alloc_rtp_frame(MAX_PAYLOAD);

                            std::memcpy(
                                tmp_frame->payload,
                                buff.frame->payload + off,
                                MAX_PAYLOAD
                            );

                            /* TODO:  */
                            /* INACTIVE(c_ts).probation.push_back(tmp_frame); */
                        }
#else
                        auto tmp_frame = kvz_rtp::frame::alloc_rtp_frame(MAX_PAYLOAD);

                        std::memcpy(
                            tmp_frame->payload,
                            buff.frame->payload + off,
                            MAX_PAYLOAD
                        );

                        /* TODO:  */
                        /* INACTIVE(c_ts).probation.push_back(tmp_frame); */
#endif
                    }
                }

                INACTIVE(c_ts).pkts_received++;
                INACTIVE(c_ts).received_size += MAX_PAYLOAD;
            }

            /* Create NAL header for the full frame using start fragments information */
            if (type == FT_START) {
                if (buff.active_ts == c_ts) {
                    buff.s_seq = c_seq;

                    buff.frame->payload[0] = (buff.hevc_ext_buf[i][0] & 0x80) | ((buff.hevc_ext_buf[i][2] & 0x3f) << 1);
                    buff.frame->payload[1] = (buff.hevc_ext_buf[i][1]);
                } else {
                    INACTIVE(c_ts).s_seq = c_seq;

                    INACTIVE(c_ts).frame->payload[1] = (buff.hevc_ext_buf[i][1]);
                    INACTIVE(c_ts).frame->payload[0] = (buff.hevc_ext_buf[i][0] & 0x80) | ((buff.hevc_ext_buf[i][2] & 0x3f) << 1);
                }
            }

            if (type == FT_END) {
                if (buff.active_ts == c_ts)
                    buff.e_seq = c_seq;
                else
                    buff.inactive[c_ts].e_seq = c_seq;
            }

            /* TODO: onko parempi hoitaa tämä? */
            if (buff.prev_f_seq == INVALID_SEQ)
                goto end;

            /* There are three types of relocations:
             *
             * 1) Relocation within read:
             *    This relocation can be done efficiently as we only need to
             *    shuffle memory around and the shuffled objects are spatially
             *    very close (at most MAX_DATAGRAMS * MAX_PAYLOAD bytes apart)
             *
             *    Relocation within read is also called shifting (defined above)
             *    and this special-case relocation has been taken care of at this 
             *    point in the execution
             *
             * 2) Relocation to other frame:
             *    This relocation must be done because the received fragment is not part
             *    of this frame
             *
             *    This relocation has also been taken care of earlier (when the timestamp mismatch
             *    with active and current frame was detected)
             *
             * 3) Relocation within frame:
             *    This is the most complex type of relocation. We need to make a note in
             *    the "reloc_needed" vector about this relocations and when TODO */
            if (buff.s_seq == INVALID_SEQ) {
                LOG_WARN("Relocation cannot be calculated");
            } else {

                /* Fragments with S-bit set don't require relocation and
                 * fragments that not part of this frame have already been 
                 * copied to correct frame (needing no further relocation for now) */
                if (type == FT_START || c_ts != buff.active_ts)
                    goto end;

                /* TODO: selitä */
                size_t off       = __calculate_offset(buff.next_off, i);
                size_t block_off = (off - 2) / MAX_PAYLOAD;

                if (block_off != c_seq - buff.s_seq) {
                    if (!sinfo.shift_needed) {
                        LOG_INFO("relocation needed for fragment: off %zu (real off %zu) index %zu", off, buff.next_off, i);
                        LOG_INFO("s_seq %u c_seq %u\n", buff.s_seq, c_seq);
                    }
                }
            }
end:
            p_seq = c_seq;
        }

        bool frame_changed = false;

        /* If we have received all fragments, the frame can be returned. */
        if (buff.s_seq != INVALID_SEQ && buff.e_seq != INVALID_SEQ) {
            if ((ssize_t)buff.pkts_received == (buff.e_seq - buff.s_seq + 1)) {

                /* TODO: resolve all relocations */

#if 0
                uint64_t diff = (uint64_t)std::chrono::duration_cast<std::chrono::microseconds>(
                    std::chrono::high_resolution_clock::now() - buff.start
                ).count();

                avg_us += diff;
                avg_us_total++;
                avg_fs += buff.frame->payload_len;
                avg_fs_total++;

                LOG_WARN("frame took %u us, %u ms, %u s (avg %u, fsize avg %u)",
                    diff,
                    diff / 1000,
                    diff / 1000 / 1000,
                    avg_us / avg_us_total,
                    avg_fs / avg_fs_total
                );
#endif

                buff.frame->payload_len = buff.received_size;
                reader->return_frame(buff.frame);

                fprintf(stderr, "------------------------------\n");

                /* TODO: tyhjennä seqs-map / swappaa mappia! */

                frame_changed = true;

                /* Frames are resolved in order meaning that the next oldest frame is resolved next */
                if (buff.tss.size() != 0) {
                    uint32_t n_ts  = __get_next_ts(buff.tss);

                    buff.active_ts     = n_ts;
                    buff.frame         = INACTIVE(n_ts).frame;
                    buff.s_seq         = INACTIVE(n_ts).s_seq;
                    buff.e_seq         = INACTIVE(n_ts).e_seq;
                    buff.pkts_received = INACTIVE(n_ts).pkts_received;
                    buff.total_size    = INACTIVE(n_ts).total_size;
                    buff.received_size = INACTIVE(n_ts).received_size;
                    buff.next_off      = INACTIVE(n_ts).next_off;
                    buff.start         = INACTIVE(n_ts).start;

                    /* Resolve all relocations (if any) 
                     * Record in relocation info vector doesn't necessarily mean that the 
                     * fragment is in wrong place/its place cannot be determined 
                     *
                     * Relocating at this point in the frame's lifetime is a delicate issue IF the fragments
                     * already received aren't contigous: TODO. selitä miksi ei voi välttämättä relokoida */
                    if (INACTIVE(n_ts).rinfo.size() != 0) {
                        /* LOG_ERROR("%zu relocations must be resolved!", INACTIVE(n_ts).rinfo.size()); */

                        uint32_t prev_seq = INACTIVE(n_ts).s_seq;

                        for (auto& i : INACTIVE(n_ts).rinfo) {
                            if (i.first - 1 != prev_seq) {
                                LOG_WARN("relocation cannot be performed, informatin missing!");
                            }

                            /* TODO: varmista että fragmentin tämänhetkinen offset on oikea (saa laskemalla prev_seqin avulla) */

                            /* fprintf(stderr, "\t%u at %zu to %zu\n", */
                            /*     i.first, */
                            /*     i.second.c_off, */
                            /*     i.second.d_off */
                            /* ); */
                            prev_seq = i.first;
                        }
                    }

                    buff.inactive.erase(n_ts);

                } else {
                    buff.total_size    = DEFAULT_ALLOC_SIZE + NAL_HDR_SIZE;
                    buff.received_size = NAL_HDR_SIZE;
                    buff.frame         = kvz_rtp::frame::alloc_rtp_frame(DEFAULT_ALLOC_SIZE + NAL_HDR_SIZE);
                    buff.s_seq         = INVALID_SEQ;
                    buff.e_seq         = INVALID_SEQ;
                    buff.pkts_received = 0;
                    buff.next_off      = 2;

                    /* TODO: selitä */
                    buff.active_ts = INVALID_TS;
                }
            }
        }

        if (sinfo.shift_needed && !frame_changed) {
            buff.next_off = sinfo.shift_offset;
            sinfo.shift_needed = false;
        }

        buff.prev_f_seq = p_seq;
    }

    return RTP_OK;
}
