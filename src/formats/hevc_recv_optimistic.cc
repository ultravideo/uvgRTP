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

#define INVALID_SEQ  0x13371338
#define INVALID_TS   0xffffffff

#ifdef _WIN32
/* For windows the MAX_DATAGRAMS is always 1, because crappy
 * Winsock interface won't allow us to read multiple packets with one system call */
#define MAX_DATAGRAMS                  1
#else

#ifdef __RTP_N_PACKETS_PER_SYSCALL__
#   define MAX_DATAGRAMS   __RTP_N_PACKETS_PER_SYSCALL__
#else
#   define MAX_DATAGRAMS   10
#endif
#endif

#ifdef __RTP_MAX_PAYLOAD__
#   define MAX_READ_SIZE   __RTP_MAX_PAYLOAD__
#else
#   define MAX_READ_SIZE   MAX_PAYLOAD
#endif

#define RTP_FRAME_MAX_DELAY          100

#define RTP_HDR_SIZE                  12
#define NAL_HDR_SIZE                   2
#define FU_HDR_SIZE                    1

#define GET_FRAME(ts)    (frames.finfo[ts])
#define ACTIVE           (frames.finfo[frames.active_ts])
#define FRAG_PSIZE(i)    (frames.headers[i].msg_len - RTP_HDR_SIZE - NAL_HDR_SIZE - FU_HDR_SIZE)

int DEFAULT_REALLOC_SIZE = 50  * MAX_PAYLOAD;
int DEFAULT_ALLOC_SIZE   = 100 * MAX_PAYLOAD + NAL_HDR_SIZE;


/* Glossary:
 *
 * fragment: A block of HEVC data carried in a packet.
 *           Doesn't in itself make a returnable HEVC frame but is rather 
 *           a part of larger frame. Sender has split the HEVC chunk into 
 *           fragments and the OFR tries to reconstruct the original HEVC chunk
 *           from these fragments.
 *
 * non-fragment: A block of data that is either invalid (invalid RTP/NAL/FU header etc.)
 *               or a block of HEVC data that is not a fragmentation unit (f.ex. VPS packet)
 *
 * packet: UDP packet read using recvmmsg/WSARecvFrom
 *         The contents are unknown, could be fragment or non-fragment */

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

/* Frame Size and Allocation Heuristics (FSAH)
 *
 * We need to keep track of both average frame and reallocation sizes
 * but also of "outlier frames" that break our assumptions and
 * require maybe even multiple relocations or have huge internal fragmentation
 *
 * This way we can try to minimize both internal fragmentation for large
 * portion of the incoming frames but also try to predict when large frame
 * is coming and try to allocate enough space for it ahead of time. 
 *
 * TODO: määritä joku ikkuna onka sisällä pakettien tulee olla ja kaikki 
 * paketit jotka ovat tämän ikkunan ulkopuolella ovat outliereita 
 *
 * tämä ikkunan koko kasvaa dynaamisesti ja adaptoituu muuttuvaan streamiin 
 * (ei pelkstää uusiin frameihin, mutta myös "vanhoihin" frame kokoihin eli 
 * jos ikkunan koko on kasvanut 20kbllä niin myös ikkunan alarajan  tulee 
 * kasvaa 20kbllä kun tämä huomataan [miten tämä huomataan?]) 
 *
 * outlierit ovat tosiaan paketteja, joiden frame koko poikkeaa normi 
 * framejen koosta ja jokta ovat harvinaisempia kuin muut framet 
 *
 * näitä outliereita ei lasketa mukaan average laskuihin */
struct fsah {

    /* Default Allocation Size */
    size_t das;

    /* Reallocation size */
    size_t ras;

    /* Average reallocation count 
     * ie. how many reallocations on average one frame undergoes
     *
     * This should be zero, or rather, the allocation scheme 
     * should strive for making this zero 
     *
     * That can be achieved by finding optimal DAS */
    size_t arc;

    /* TODO */
    std::unordered_map<int, size_t> outliers;
};

struct reloc_info {
    size_t c_off;   /* current offset in the frame */
    size_t size;    /* fragment size */
    void *ptr;      /* pointer to memory (reloc_type tells where the ptr is pointing to) */
    int reloc_type; /* relocation type (see RELOC_TYPES) */
};

struct shift_info {
    bool shift_needed;
    size_t shift_offset;
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

struct frame_info {
    /* One big frame for all fragments, this is resized if all space is consumed */
    kvz_rtp::frame::rtp_frame *frame;

    size_t total_size;    /* allocated size */
    size_t received_size; /* used size */
    size_t pkts_received; /* # packets received (used to detect when all fragments have been received) */

    size_t reallocs;      /* how many reallocations has this frame undergone */
    size_t relocs;        /* how many copy operations was performed on this frame (both shifting and relocation) */

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

#ifdef _WIN32
#error "windows not yet supported"
#else
    struct mmsghdr headers[MAX_DATAGRAMS];

    /* * 3 because RTP header, HEVC NAL and FU and payload are read into these buffers 
     * (or rather pointers to these buffers are stored into iovec) */
    struct iovec iov[MAX_DATAGRAMS * 3];
#endif

    /* timestamp of the oldest but still active frame, used to index finfo 
     *
     * Active is defined as not all fragments have been received but the oldest
     * fragment of the frame is not RTP_FRAME_MAX_DELAY milliseconds old */
    uint32_t active_ts;

    /* Total number of frames received */
    size_t nframes;

    /* Frames are handled FIFO style so keep the timestamps in a queue */
    std::queue<uint32_t> tss;

    /* Size and allocation heuristics, updated with every fragment */
    struct fsah fsah;

    /* Shifting information */
    struct shift_info sinfo;

    /* All active and inactive frames */
    std::unordered_map<uint32_t, struct frame_info> finfo;

    uint32_t prev_f_seq;

    size_t total_received;
    size_t total_copied;
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
 * For empty frames the start offet ("start") might be NAL_HDR_SIZE 
 * (no fragments have been received) so we need to take that 
 * into consideration when calculating the offset */
static inline size_t __calculate_offset(size_t start, size_t index)
{
    size_t off = index * MAX_PAYLOAD;

    if (start > NAL_HDR_SIZE)
        off = start - MAX_DATAGRAMS * MAX_PAYLOAD + off;
    else
        off += NAL_HDR_SIZE;

    return off;
}

static inline void __init_headers(
    kvz_rtp::frame::rtp_frame *frame,
    kvz_rtp::frame::rtp_header *header,
    uint8_t *hevc_ext_buf
)
{
    frame->payload[0] = (hevc_ext_buf[0] & 0x80) | ((hevc_ext_buf[2] & 0x3f) << 1);
    frame->payload[1] = (hevc_ext_buf[1]);

    std::memcpy(&frame->header, header, RTP_HDR_SIZE);
}

static inline void __add_relocation_entry(struct frame_info& finfo, uint16_t seq, void *mem, size_t size, int reloc_type)
{
    struct reloc_info reloc = {
        finfo.next_off,  /* current offset */
        size,            /* fragment size */
        mem,             /* pointer to fragment */
        reloc_type,      /* type of relocation needed for the frame */
    };

    finfo.rinfo.insert(std::make_pair(seq, reloc));
}

static void __relocate_temporarily(struct frame_info& finfo, uint16_t seq, size_t size, size_t offset)
{
#ifdef __RTP_USE_PROBATION_ZONE__
    if (finfo.frame->probation_off != finfo.frame->probation_len) {
        void *ptr = finfo.frame->probation + finfo.frame->probation_off;

        finfo.relocs++;

        std::memcpy(ptr, finfo.frame->payload + offset, size);
        __add_relocation_entry(finfo, seq, ptr, size, RT_PROBATION);

        finfo.frame->probation_off += size;
    } else {
#endif
        auto tmp_frame = kvz_rtp::frame::alloc_rtp_frame(MAX_PAYLOAD);
        finfo.relocs++;

        std::memcpy(tmp_frame->payload, finfo.frame->payload + offset, size);
        __add_relocation_entry(finfo, seq, tmp_frame, size, RT_FRAME);

        /* GET_FRAME(c_ts).probation.push_back(tmp_frame); */

#ifdef __RTP_USE_PROBATION_ZONE__
    }
#endif
}

static void __reallocate_frame(struct frames& frames, uint32_t timestamp)
{

    /* LOG_ERROR("PENALIZE!"); */

    auto tmp_frame = kvz_rtp::frame::alloc_rtp_frame(GET_FRAME(timestamp).total_size + frames.fsah.ras);

    std::memcpy(tmp_frame->payload, GET_FRAME(timestamp).frame->payload, GET_FRAME(timestamp).total_size);

    (void)kvz_rtp::frame::dealloc_frame(GET_FRAME(timestamp).frame);

    GET_FRAME(timestamp).frame       = tmp_frame;
    GET_FRAME(timestamp).total_size += frames.fsah.ras;
    frames.fsah.ras += 20 * 1024;
}

/* TODO: make this a member function? */
static void __create_frame_entry(struct frames& frames, uint32_t timestamp, void *payload, size_t size)
{
    GET_FRAME(timestamp).frame         = kvz_rtp::frame::alloc_rtp_frame(frames.fsah.das);
    GET_FRAME(timestamp).total_size    = frames.fsah.das;
    GET_FRAME(timestamp).next_off      = NAL_HDR_SIZE;
    GET_FRAME(timestamp).received_size = NAL_HDR_SIZE;
    GET_FRAME(timestamp).pkts_received = 0;
    GET_FRAME(timestamp).s_seq         = INVALID_SEQ;
    GET_FRAME(timestamp).e_seq         = INVALID_SEQ;
}

rtp_error_t __hevc_receiver_optimistic(kvz_rtp::reader *reader)
{
    LOG_INFO("in optimisic hevc receiver");

    struct frames frames;

    frames.active_ts = INVALID_TS;
    frames.nframes   = 0;

    frames.fsah.das = DEFAULT_ALLOC_SIZE;
    frames.fsah.ras = DEFAULT_REALLOC_SIZE;
    frames.fsah.arc = 0;

    /* We need to use INVALID_TS as bootstrap timestamp
     * because we don't know the timestamp of incoming frame */
    frames.active_ts = INVALID_TS;
    __create_frame_entry(frames, INVALID_TS, NULL, 0);

    std::memset(frames.headers, 0, sizeof(frames.headers));

    int k = 0, nread = 0;

    uint64_t total_proc_time = 0;
    uint64_t total_frames_recved = 0;

    while (reader->active()) {


        if (ACTIVE.next_off + MAX_DATAGRAMS * MAX_PAYLOAD > ACTIVE.total_size)
            __reallocate_frame(frames, frames.active_ts);

        for (size_t i = 0, k = 0; i < MAX_DATAGRAMS; ++i, k += 3) {
#ifdef __linux__
            frames.iov[k + 0].iov_base = &frames.rtp_headers[i];
            frames.iov[k + 0].iov_len  = sizeof(frames.rtp_headers[i]);

            frames.iov[k + 1].iov_base = &frames.hevc_ext_buf[i];
            frames.iov[k + 1].iov_len  = 3;

            frames.iov[k + 2].iov_base = ACTIVE.frame->payload + ACTIVE.next_off;
            frames.iov[k + 2].iov_len  = MAX_PAYLOAD;
            ACTIVE.next_off           += MAX_PAYLOAD;

            frames.headers[i].msg_hdr.msg_iov    = &frames.iov[k];
            frames.headers[i].msg_hdr.msg_iovlen = 3;
#endif
        }

#ifdef _WIN32
        if (WSARecvFrom(reader->get_raw_socket(), frame)) {
        }
#else
        if ((nread = recvmmsg(reader->get_raw_socket(), frames.headers, MAX_DATAGRAMS, 0, nullptr)) < 0) {
            LOG_ERROR("recvmmsg() failed, %s", strerror(errno));
            continue;
        }
#endif

        /* if ((++k) % 100 == 0) { */
        /*     fprintf(stderr, "%zu total packets, %zu copied packets\n", frames.total_received, frames.total_copied); */

        /*     if (total_frames_recved > 0) */
        /*         fprintf(stderr, "frame avg processing time: %lf us\n", (double)total_proc_time / (double)total_frames_recved); */
        /* } */

        uint32_t p_seq = INVALID_SEQ;

        frames.sinfo.shift_needed = false;
        frames.sinfo.shift_offset = 0;

        for (size_t i = 0; i < MAX_DATAGRAMS; ++i) {
            int type = __get_frame_type(frames.hevc_ext_buf[i]);

            frames.rtp_headers[i].timestamp = ntohl(frames.rtp_headers[i].timestamp);
            frames.rtp_headers[i].seq       = ntohs(frames.rtp_headers[i].seq);

            uint32_t c_ts  = frames.rtp_headers[i].timestamp;
            uint32_t c_seq = frames.rtp_headers[i].seq;

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
            if (frames.sinfo.shift_needed) {
                size_t c_off = __calculate_offset(ACTIVE.next_off, i);

                /* We don't need to shift non-fragments and invalid data
                 * because non-fragments will be copied to other frame from their currenct position
                 * in the frame and both non-fragments and invalid packets will be overwritten */
                if (type != FT_NOT_FRAG && type != FT_INVALID) {
                    ACTIVE.relocs++;
                    std::memcpy(
                        ACTIVE.frame->payload + frames.sinfo.shift_offset,
                        ACTIVE.frame->payload + c_off,
                        MAX_PAYLOAD
                    );
                }
            }

            if (type == FT_NOT_FRAG) {
                size_t len = frames.headers[i].msg_len - RTP_HDR_SIZE;
                auto frame = kvz_rtp::frame::alloc_rtp_frame(len);
                size_t off = __calculate_offset(ACTIVE.next_off, i);

                /* If previous packet set "shift_needed" to true, we don't need to
                 * do anything here because the shift offset will point to that packets's
                 * start where the first fragment should be copied.
                 * Eventually, through appending shifts, this non-fragment will be overwritten too
                 *
                 * If this is the first non-fragment, it will trigger a series of overwriting shift
                 * by setting the "shift_needed" to true and shift_offset to its own offset (which means 
                 * that next shift will overwrite this packet) */
                if (!frames.sinfo.shift_needed) {
                    frames.sinfo.shift_needed = true;
                    frames.sinfo.shift_offset = off;
                }

                std::memcpy(frame->payload + 0, &frames.hevc_ext_buf[i], 3);
                /* fprintf(stderr, "%u: %d (%zu) | %d (%zu)\n", frames.headers[i].msg_len, off, off, len, len); */
                /* fprintf(stderr, "n packets read: %d\n", nread); */
                std::memcpy(frame->payload + 3, ACTIVE.frame->payload + off, len - 3);

                reader->return_frame(frame);
                continue;
            }

            if (type == FT_INVALID) {
                /* TODO: update shift info */
                LOG_WARN("Invalid frame received!");
                for (;;);
                continue;
            }

            /* TODO: selitä */
            if (frames.active_ts == INVALID_TS) {
                frames.finfo[c_ts] = frames.finfo[frames.active_ts];
                frames.active_ts   = c_ts;

                frames.finfo.erase(INVALID_TS);
                GET_FRAME(c_ts).start = kvz_rtp::clock::hrc::now();
            }

            if (frames.active_ts == c_ts) {
                ACTIVE.pkts_received += 1;
                ACTIVE.received_size += FRAG_PSIZE(i);

                if (frames.sinfo.shift_needed) {
                    /* we have shifted the memory to correct place above but we need to
                     * update the offset. Because this fragment belongs to this frame, 
                     * next shift (if there are fragments left) must be appending instead of overwriting shift */
                    frames.sinfo.shift_offset += FRAG_PSIZE(i);
                }

            /* This packet doesn't belong to active frame and it must be copied to some other frame */
            } else {
                size_t off = __calculate_offset(ACTIVE.next_off, i);

                /* i + 1 < MAX_DATAGRAMS means that this packet is not the last packet of the read
                 * and we need to overwrite it. Set the "shift_offset" to this packet's offset to 
                 * cause an overwriting shift */
                if (i + 1 < MAX_DATAGRAMS) {
                    frames.sinfo.shift_needed = true;
                    frames.sinfo.shift_offset = off;
                }

                /* This fragment is part of a future frame and it's the first fragment of that frame,
                 * create new frame entry and push the timestamp to queue 
                 *
                 * The happy case is that this first fragment is also the start fragment and we can
                 * do the frame setup for the future frame here. 
                 *
                 * Most likely this fragment is not the first fragment (but one of the first fragments)
                 * in which case we need to copy the fragment to probation zone and resolve the relocation
                 * when the fragment with S-bit set is receive 
                 *
                 * Regardless of the fragment type, the clock is started now and all fragments of this
                 * frame must be received withing RTP_FRAME_MAX_DELAY milliseconds */
                if (frames.finfo.find(c_ts) == frames.finfo.end()) {
                    frames.tss.push(c_ts);

                    __create_frame_entry(frames, c_ts, NULL, 0);
                    GET_FRAME(c_ts).start = kvz_rtp::clock::hrc::now();

                    if (type == FT_START) {
                        GET_FRAME(c_ts).s_seq = c_seq;
                        GET_FRAME(c_ts).relocs++;

                        std::memcpy(
                            GET_FRAME(c_ts).frame->payload + NAL_HDR_SIZE,
                            ACTIVE.frame->payload + off,
                            FRAG_PSIZE(i)
                        );
                    } else {
                        LOG_WARN("frame must be copied to probation zone");
                    }

                /* This is not the first fragment of an inactive frame, copy it to correct frame
                 * or to probation area if its place cannot be determined (s_seq must be known) */
                } else {
                    if (GET_FRAME(c_ts).s_seq != INVALID_SEQ) {

                        /* TODO: THERE HAS TO BE NO GAPS BETWEEN S_SEQ AND THIS FRAGMENT 
                         *       OTHERWISE -> PROBATION ZONE */
                        /* LOG_WARN("make sure there's no gaps"); */

                        if (GET_FRAME(c_ts).next_off + FRAG_PSIZE(i) > GET_FRAME(c_ts).total_size)
                            __reallocate_frame(frames, c_ts);

                        std::memcpy(
                            GET_FRAME(c_ts).frame->payload + GET_FRAME(c_ts).next_off,
                            ACTIVE.frame->payload + off,
                            FRAG_PSIZE(i)
                        );
                        GET_FRAME(c_ts).relocs++;

                        /* __add_relocation_entry(GET_FRAME(c_ts), c_seq, NULL, 0, RT_PAYLOAD); */
                    } else {
                        LOG_WARN("relocate to probation zone");
                        /* Relocate the fragment temporarily to probation zone/temporary frame */
                        /* __relocate_temporarily(GET_FRAME(c_ts), c_seq, FRAG_PSIZE(i), off); */
                    }
                }

                GET_FRAME(c_ts).next_off      += FRAG_PSIZE(i);
                GET_FRAME(c_ts).received_size += FRAG_PSIZE(i);
                GET_FRAME(c_ts).pkts_received += 1;
            }

            /* Create NAL header for the full frame using start fragments information */
            if (type == FT_START) {
                GET_FRAME(c_ts).s_seq = c_seq;
                __init_headers(GET_FRAME(c_ts).frame, &frames.rtp_headers[i], frames.hevc_ext_buf[i]);
            }

            if (type == FT_END)
                GET_FRAME(c_ts).e_seq = c_seq;

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
            if (ACTIVE.s_seq == INVALID_SEQ) {
                LOG_WARN("Relocation cannot be calculated");
                /* Relocate the fragment temporarily to probation zone/temporary frame */
                /* size_t off = __calculate_offset(ACTIVE.next_off, i); */
                /* __relocate_temporarily(GET_FRAME(c_ts), c_seq, FRAG_PSIZE(i), off); */
            } else {

                /* Fragments with S-bit set don't require relocation and
                 * fragments that not part of this frame have already been 
                 * copied to correct frame (needing no further relocation for now) */
                if (type == FT_START || c_ts != frames.active_ts)
                    goto end;

                /* TODO: selitä */
                size_t off       = __calculate_offset(ACTIVE.next_off, i);
                size_t block_off = (off - 2) / MAX_PAYLOAD;

                if (block_off != c_seq - ACTIVE.s_seq) {
                    if (!frames.sinfo.shift_needed) {
                        LOG_INFO("relocation needed for fragment: off %zu (real off %zu) index %zu", off, ACTIVE.next_off, i);
                        LOG_INFO("s_seq %u c_seq %u\n", ACTIVE.s_seq, c_seq);
                    }
                }
            }
end:
            p_seq = c_seq;
        }

        /* There are two cases why the active frame is changed: 
         *  - either we've received every fragment of the frame and the frame can be returned to user  
         *  - the time window within which all fragments must be received has closed 
         *
         * The first case will remove all frame related extra info and return the actual RTP frame to user
         * The second case will move the active frame to inactive hashmap */
        bool change_active_frame = false;

        if (kvz_rtp::clock::hrc::diff_now(ACTIVE.start) > RTP_FRAME_MAX_DELAY) {
            /* LOG_WARN("deadline missed for frame"); */
            change_active_frame = true;
        }

        if (ACTIVE.s_seq != INVALID_SEQ && ACTIVE.e_seq != INVALID_SEQ) {
            if ((ssize_t)ACTIVE.pkts_received == (ACTIVE.e_seq - ACTIVE.s_seq + 1)) {

                total_proc_time     += kvz_rtp::clock::hrc::diff_now_us(ACTIVE.start);
                total_frames_recved += 1;

                if (ACTIVE.rinfo.size() != 0) {
                    LOG_ERROR("resolve relocations");
                }

                /* TODO: resolve all relocations (if any) */

                frames.total_received += ACTIVE.pkts_received;
                frames.total_copied   += ACTIVE.relocs;

                /* fprintf(stderr, "received %zu, copied %u\n", ACTIVE.pkts_received, ACTIVE.relocs); */
                ACTIVE.frame->payload_len = ACTIVE.received_size;
                reader->return_frame(ACTIVE.frame);
                frames.finfo.erase(frames.active_ts);
                change_active_frame = true;
            }
        }

        if (change_active_frame) {
            /* Frames are resolved in order meaning that the next oldest frame is resolved next */
            if (frames.tss.size() != 0) {
                frames.active_ts = __get_next_ts(frames.tss);

                /* Resolve all relocations (if any) 
                 * Record in relocation info vector doesn't necessarily mean that the 
                 * fragment is in wrong place/its place cannot be determined 
                 *
                 * Relocating at this point in the frame's lifetime is a delicate issue IF the fragments
                 * already received aren't contigous: TODO. selitä miksi ei voi välttämättä relokoida */
                if (ACTIVE.rinfo.size() != 0) {
                    /* LOG_ERROR("%zu relocations must be resolved!", GET_FRAME(frames.active_ts).rinfo.size()); */

                    uint32_t prev_seq = ACTIVE.s_seq;

                    for (auto& i : ACTIVE.rinfo) {
                        if (i.first - 1 != (ssize_t)prev_seq) {
                            LOG_WARN("relocation cannot be performed, informatin missing!");
                        }

                        if (i.second.reloc_type == RT_PROBATION)
                            fprintf(stderr, "probation area\n");
                        else if (i.second.reloc_type == RT_FRAME)
                            fprintf(stderr, "separate frame\n");
                        prev_seq = i.first;
                    }

                    ACTIVE.rinfo.clear();
                }

            } else {
                /* Previous (returned) framed was received so that "# of fragments" % MAX_DATAGRAMS == 0 
                 *
                 * This means that we have no knowledge of the next frame and we must set the active_ts
                 * to INVALID_TS and deal with the timestamp relocation was once we know better */
                __create_frame_entry(frames, INVALID_TS, NULL, 0);
                frames.active_ts = INVALID_TS;
            }
        }

        /* This read caused some shifting to happen which means that the next free slot is
         * actually below "next_off" and we must update it so there are no empty slots in the frame */
        if (frames.sinfo.shift_needed && !change_active_frame) {
            ACTIVE.next_off = frames.sinfo.shift_offset;
            frames.sinfo.shift_needed = false;
        }
    }

    return RTP_OK;
}
