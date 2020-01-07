#include <cstdint>
#include <cstring>
#include <iostream>
#include <queue>
#include <unordered_map>
#include <unordered_set>

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
#   define MAX_DATAGRAMS   1
#endif
#endif

#ifdef __RTP_MAX_PAYLOAD__
#   define MAX_READ_SIZE   __RTP_MAX_PAYLOAD__
#else
#   define MAX_READ_SIZE   MAX_PAYLOAD
#endif

#define RTP_FRAME_MAX_DELAY           50

#define RTP_HDR_SIZE                  12
#define NAL_HDR_SIZE                   2
#define FU_HDR_SIZE                    1

#define GET_FRAME(ts)    (frames.finfo[ts])
#define ACTIVE           (frames.finfo[frames.active_ts])
#define FRAG_PSIZE(i)    (frames.headers[i].msg_len - RTP_HDR_SIZE - NAL_HDR_SIZE - FU_HDR_SIZE)

int DEFAULT_REALLOC_SIZE = 100 * MAX_READ_SIZE;
int DEFAULT_ALLOC_SIZE   = 100 * MAX_READ_SIZE + NAL_HDR_SIZE;


/* Glossary:
 *
 * fragment: A block of HEVC data carried in a packet.
 *           Doesn't in itself make a returnable HEVC frame but is rather
 *           part of a larger frame. Sender has split the HEVC chunk into
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
    size_t alloc_size;

    /* Reallocation size */
    size_t ras;
    size_t realloc_size;

    /* Average reallocation count
     * ie. how many reallocations on average one frame undergoes
     *
     * This should be zero, or rather, the allocation scheme
     * should strive for making this zero
     *
     * That can be achieved by finding optimal DAS */
    size_t arc;
    size_t avg_realloc_cnt;

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
    std::vector<kvz_rtp::frame::rtp_frame *> probation;

    /* Store all received sequence numbers here so we can detect duplicate packets */
    std::unordered_set<uint32_t> seqs;

    /* start and end sequences of the frame (frames with S/E bit set) 
     * and the sequence number of the latest framgment copied to frame */
    uint32_t s_seq;
    uint32_t e_seq;
    uint32_t last_seq;

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

    /* Map of frames (timestamps) and the sequence number of last fragment.
     * This map is used when calculating relocation info for fragments that 
     * are part of frames whose start sequence is not known 
     *
     * If the frame is complete (or its only/last framgment is received), this
     * map's entry for that frame is updated, othewise it's set to INVALID_SEQ 
     *
     * This way, when a fragment that should be relocated to probation is received, 
     * we can check this map to check whether the framgment can be relocated to frame */
    std::map<uint32_t, uint16_t> all_seqs;

    /* Keep track of late frames so that they can be discarded without further processing */
    std::unordered_map<uint32_t, kvz_rtp::clock::hrc::hrc_t> late_frames;

    /* Sequence number of the previous frame/previous frames's last fragment
     *
     * We can do relocations for inactive frames even if the frame's start sequence
     * is missing if we know the end sequence of previous frame
     * (because the start sequence is just prev_eseq + 1)
     *
     * There is, however, a possibility that prev_eseq points to an even older
     * frame than previous (maybe previous frame's previous frame but the frames
     * are out of order) which means that our relocation calculations may be incorrect.
     *
     * That is why relocations based on prev_eseq are not definitive and they will leave
     * a mark in the relocation table meaning they must all be resolved before the frame
     * becomes active
     *
     * Even though there's a risk of incorrect relocations, it's still better to assume
     * that prev_eseq is correct because otherwise we are going to overpopulate probation
     * zone (especially when the number of fragments per frame is large)
     * leading to scattered memory layout for fragments and larger amount of "unnecessary" copies
     * to be done when the frame is activated */
    uint32_t prev_eseq;

    /* Sequence number of the previously handled fragment, could be part of this frame, could be part of some other frame */
    uint32_t prevr_eseq;

    /* These are just for bookkeeping */
    size_t total_received;
    size_t total_bytes_received;
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
    size_t off = index * MAX_READ_SIZE;

    if (start > NAL_HDR_SIZE)
        off = start - MAX_DATAGRAMS * MAX_READ_SIZE + off;
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

static void __relocate_temporarily(struct frame_info& src, struct frame_info& dst, uint16_t seq, size_t size, size_t offset)
{
#ifdef __RTP_USE_PROBATION_ZONE__
    if (dst.frame->probation_off != dst.frame->probation_len) {
        void *ptr = dst.frame->probation + dst.frame->probation_off;

        src.relocs++;

        std::memcpy(ptr, src.frame->payload + offset, size);
        __add_relocation_entry(dst, seq, ptr, size, RT_PROBATION);

        dst.frame->probation_off += size;
    } else {
#endif
        auto tmp_frame = kvz_rtp::frame::alloc_rtp_frame(size);
        src.relocs++;

        std::memcpy(tmp_frame->payload, src.frame->payload + offset, size);
        __add_relocation_entry(dst, seq, tmp_frame, size, RT_FRAME);

#ifdef __RTP_USE_PROBATION_ZONE__
    }
#endif
}

static void __reallocate_frame(struct frames& frames, uint32_t timestamp)
{
    auto tmp_frame = kvz_rtp::frame::alloc_rtp_frame(GET_FRAME(timestamp).total_size + frames.fsah.ras);

    std::memcpy(tmp_frame->payload, GET_FRAME(timestamp).frame->payload, GET_FRAME(timestamp).total_size);
    (void)kvz_rtp::frame::dealloc_frame(GET_FRAME(timestamp).frame);

    GET_FRAME(timestamp).frame       = tmp_frame;
    GET_FRAME(timestamp).total_size += frames.fsah.ras;

    frames.fsah.ras += 20 * 1024;
    frames.fsah.das += 20 * 1024;
}

static void __create_frame_entry(struct frames& frames, uint32_t timestamp, void *payload, size_t size)
{
    (void)payload, (void)size;

    GET_FRAME(timestamp).frame         = kvz_rtp::frame::alloc_rtp_frame(frames.fsah.das);
    GET_FRAME(timestamp).total_size    = frames.fsah.das;
    GET_FRAME(timestamp).next_off      = NAL_HDR_SIZE;
    GET_FRAME(timestamp).received_size = NAL_HDR_SIZE;
    GET_FRAME(timestamp).pkts_received = 0;
    GET_FRAME(timestamp).s_seq         = INVALID_SEQ;
    GET_FRAME(timestamp).e_seq         = INVALID_SEQ;
    GET_FRAME(timestamp).last_seq      = INVALID_SEQ;
}

static void __resolve_relocations(struct frames& frames, uint32_t ts)
{
    if (GET_FRAME(ts).rinfo.size() == 0)
        return;

    /* LOG_ERROR("%zu relocations must be resolved (total received %zu)!", */
    /*     GET_FRAME(ts).rinfo.size(), GET_FRAME(ts).pkts_received); */

    if (GET_FRAME(ts).total_size < GET_FRAME(ts).rinfo.size() * MAX_READ_SIZE)
        __reallocate_frame(frames, frames.active_ts);

    /* TODO: yritä käyttää prev_eseqiä offsetin laskemiseen! 
     *
     * TAI! käytä mappia jossa kaikki saadut seqit ja selvitä sieltä mikä on ensimmäisen
     * relocation fragmentin ja edellisen framen välinen "välimatka" sekvenssinumeroissa! */
    size_t off        = (GET_FRAME(ts).s_seq != INVALID_SEQ) ? NAL_HDR_SIZE : MAX_READ_SIZE + NAL_HDR_SIZE;
    uint32_t prev_seq = INVALID_SEQ;

    for (auto& i : GET_FRAME(ts).rinfo) {

        /* The relocation table is in order so the smallest sequence number is first and so on.
         * Some of the fragments may have not have been received yet, so we can't just willy-nilly
         * copy them in the order which they're in the relocation table but rather we need calculate
         * offset for each fragments and copy it there. This leaves empty slots in the frame which
         * can be later on filled when the correct fragments are received */
        if (prev_seq != INVALID_SEQ)
            off = off + (i.first - prev_seq) * MAX_READ_SIZE;

        switch (i.second.reloc_type) {
            case RT_PAYLOAD:
                /* TODO: make sure the fragment has been copied to correct place */
                break;

            case RT_PROBATION:
                std::memcpy(GET_FRAME(ts).frame->payload + off, i.second.ptr, i.second.size);
                break;

            case RT_FRAME:
            {
                auto frame__ = (kvz_rtp::frame::rtp_frame *)i.second.ptr;
                std::memcpy(GET_FRAME(ts).frame->payload + off, frame__->payload, i.second.size);
                (void)kvz_rtp::frame::dealloc_frame(frame__);
            }
            break;
        }

        prev_seq = i.first;
    }

    GET_FRAME(ts).rinfo.clear();
}

rtp_error_t __hevc_receiver_optimistic(kvz_rtp::reader *reader)
{
    /* LOG_INFO("Starting Optimistic HEVC Fragment Receiver..."); */

    struct frames frames;

    frames.prev_eseq  = INVALID_SEQ;
    frames.prevr_eseq = INVALID_SEQ;
    frames.active_ts  = INVALID_TS;
    frames.nframes    = 0;

    frames.fsah.das   = DEFAULT_ALLOC_SIZE;
    frames.fsah.ras   = DEFAULT_REALLOC_SIZE;
    frames.fsah.arc   = 0;

    frames.total_received = 0;
    frames.total_copied   = 0;

    /* We need to use INVALID_TS as bootstrap timestamp
     * because we don't know the timestamp of incoming frame */
    frames.active_ts = INVALID_TS;
    __create_frame_entry(frames, INVALID_TS, NULL, 0);

    std::memset(frames.headers, 0, sizeof(frames.headers));

    int nread = 0;

    uint64_t total_proc_time = 0;
    uint64_t total_frames_recved = 0;

    while (reader->active()) {

        if (ACTIVE.next_off + MAX_DATAGRAMS * MAX_READ_SIZE > ACTIVE.total_size)
            __reallocate_frame(frames, frames.active_ts);

        for (size_t i = 0, k = 0; i < MAX_DATAGRAMS; ++i, k += 3) {
#ifdef __linux__
            frames.iov[k + 0].iov_base = &frames.rtp_headers[i];
            frames.iov[k + 0].iov_len  = sizeof(frames.rtp_headers[i]);

            frames.iov[k + 1].iov_base = &frames.hevc_ext_buf[i];
            frames.iov[k + 1].iov_len  = 3;

            frames.iov[k + 2].iov_base = ACTIVE.frame->payload + ACTIVE.next_off;
            frames.iov[k + 2].iov_len  = MAX_READ_SIZE;
            ACTIVE.next_off           += MAX_READ_SIZE;

            frames.headers[i].msg_hdr.msg_iov    = &frames.iov[k];
            frames.headers[i].msg_hdr.msg_iovlen = 3;
#endif
        }

#ifdef _WIN32
        if (WSARecvFrom(reader->get_raw_socket(), frame)) {
        }
#else
        if ((nread = recvmmsg(reader->get_raw_socket(), frames.headers, MAX_DATAGRAMS, MSG_WAITFORONE, nullptr)) < 0) {
            LOG_ERROR("recvmmsg() failed, %s", strerror(errno));
            continue;
        }
#endif

        frames.sinfo.shift_needed = false;
        frames.sinfo.shift_offset = 0;

        for (int i = 0; i < nread; ++i) {
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
                    std::memcpy(
                        ACTIVE.frame->payload + frames.sinfo.shift_offset,
                        ACTIVE.frame->payload + c_off,
                        FRAG_PSIZE(i)
                    );

                    ACTIVE.relocs++;
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

                std::memcpy(&frame->header,     &frames.rtp_headers[i],  sizeof(frames.rtp_headers[i]));
                std::memcpy(frame->payload + 0, &frames.hevc_ext_buf[i], 3);
                std::memcpy(frame->payload + 3, ACTIVE.frame->payload + off, len - 3);

                frames.prev_eseq = frames.prevr_eseq = c_seq;
                reader->return_frame(frame);
                continue;
            }

            if (type == FT_INVALID) {
                /* TODO: update shift info */
                LOG_WARN("Invalid frame received!");
                for (;;);
                continue;
            }

            /* This fragment is way too late and should not be processed at all because
             * we have no use for it at this point.
             *
             * Because it's considered garbage, an overwriting shift must take place
             * so the actual active frames stays valid */
            if (frames.late_frames.find(c_ts) != frames.late_frames.end()) {
                if (!frames.sinfo.shift_needed) {
                    frames.sinfo.shift_needed = true;
                    frames.sinfo.shift_offset = __calculate_offset(ACTIVE.next_off, i);
                }
                continue;
            }

            /* When reading fragments using recvmmsg() or WSARecvFrom(), it's possible that
             * the previously returned frame was read so that we didn't get any "spill-over fragments"
             * (fragments that belong to future frame). If this is the case, when the next recvmmsg()/WSARecvFrom()
             * is called, we know nothing about the frame we're about to read (f.ex. its timestamp)
             *
             * For these somewhat rare cases (especially when reading multiple fragments per syscall),
             * the OFR uses INVALID_TS as magic value to indicate that frame's timestamp was not known.
             * But now that the fragments have been read, we can relocate this frame to its correct place
             * (c_ts) in the finfo map and start the timer */
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
                        std::memcpy(
                            GET_FRAME(c_ts).frame->payload + NAL_HDR_SIZE,
                            ACTIVE.frame->payload + off,
                            FRAG_PSIZE(i)
                        );

                        GET_FRAME(c_ts).s_seq     = c_seq;
                        GET_FRAME(c_ts).relocs   += 1;
                        GET_FRAME(c_ts).next_off += FRAG_PSIZE(i);

                        if (GET_FRAME(c_ts).rinfo.size() > 0) {
                            LOG_ERROR("resolve relocations!");
                        }
                    } else {
                        /* Relocate the fragment temporarily to probation zone/temporary frame */
                        __relocate_temporarily(ACTIVE, GET_FRAME(c_ts), c_seq, FRAG_PSIZE(i), off);
                    }

                /* This is not the first fragment of an inactive frame, copy it to correct frame
                 * or to probation area if its place cannot be determined (s_seq must be known) */
                } else {
                    if (GET_FRAME(c_ts).s_seq != INVALID_SEQ) {
                        if (GET_FRAME(c_ts).next_off + FRAG_PSIZE(i) > GET_FRAME(c_ts).total_size)
                            __reallocate_frame(frames, c_ts);

                        /* TODO: make sure the fragment can be copied safely */

                        std::memcpy(
                            GET_FRAME(c_ts).frame->payload + GET_FRAME(c_ts).next_off,
                            ACTIVE.frame->payload + off,
                            FRAG_PSIZE(i)
                        );

                        GET_FRAME(c_ts).relocs   += 1;
                        GET_FRAME(c_ts).next_off += FRAG_PSIZE(i);

                    } else {
                        /* Relocate the fragment temporarily to probation zone/temporary frame */
                        __relocate_temporarily(ACTIVE, GET_FRAME(c_ts), c_seq, FRAG_PSIZE(i), off);
                    }
                }

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

            /* Here we must now check that during this read, all fragments were read to correct place 
             *
             * If we detect, for example, that a fragment 3 was read to fragment 2's place, we need to shift 
             * all fragments forward. */
            if (c_ts == frames.active_ts) {
                if (c_seq - 1 != ACTIVE.last_seq && (ACTIVE.last_seq - 0xffff != c_seq) && ACTIVE.last_seq != INVALID_SEQ) {
                    /* TODO: this is going to be very ugly */
                    /* LOG_ERROR("error!!! %u %u", c_seq, ACTIVE.last_seq); */
                }

                ACTIVE.last_seq = c_seq;
            } else {
                /* This fragment doens't belong to active frame so it must cause an overwriting shift */
                if (!frames.sinfo.shift_needed) {
                    frames.sinfo.shift_needed = true;
                    frames.sinfo.shift_offset = __calculate_offset(ACTIVE.next_off, i);
                }
            }

            frames.prevr_eseq = c_seq;
        } /* end of for loop */

        /* In typical video conferencing situation, we receive a burst of packets every N milliseconds.
         * Because the amount of packets read using recvmmsg(2) is configurable, a situation might arise
         * where we've read every packet of the burst but the amount of packets is less than what recvmmsg(2)
         * is supposed to read.
         *
         * For example, let's say the packets per system call is 5 but a burst of packets from remote contains
         * only 4 packets. If we didn't use MSG_WAITFORONE, the syscall would block until all 5 packets were read,
         * and the last packet would be part of next burst. This would make our current frame to be late and cause
         * distrotions during decoding.
         *
         * During buffer initialization, however, we assume we'll be reading 5 packets and update ACTIVE.next_off
         * according to this assumption. Here we must fix the offset pointer if we read fewer packets */
        if (nread < MAX_DATAGRAMS) {
            ACTIVE.next_off -= (MAX_READ_SIZE * (MAX_DATAGRAMS - nread));
        }

        /* There are two cases why the active frame is changed:
         *  - either we've received every fragment of the frame and the frame can be returned to user
         *  - the time window within which all fragments must be received has closed
         *
         * The first case will remove all frame related extra info and return the actual RTP frame to user
         * The second case will move the active frame to inactive hashmap */
        bool change_active_frame = false;

        if (kvz_rtp::clock::hrc::diff_now(ACTIVE.start) > RTP_FRAME_MAX_DELAY && ACTIVE.pkts_received > 0) {
            fprintf(stderr, "\nframe %u deadline missed! (%zu ms)\n", frames.active_ts, kvz_rtp::clock::hrc::diff_now(ACTIVE.start));
            fprintf(stderr, "s_seq 0x%x | e_seq 0x%x\n", ACTIVE.s_seq, ACTIVE.e_seq);

            frames.late_frames.insert(std::make_pair(frames.active_ts, ACTIVE.start));
            change_active_frame = true;
        }

        if (ACTIVE.s_seq != INVALID_SEQ && ACTIVE.e_seq != INVALID_SEQ) {
            size_t pkts_received = 0;

            if (ACTIVE.s_seq > ACTIVE.e_seq)
                pkts_received = 0xffff - ACTIVE.s_seq + ACTIVE.e_seq + 2;
            else
                pkts_received = ACTIVE.e_seq - ACTIVE.s_seq + 1;

            /* fprintf(stderr, "%zu missing\n", pkts_received - ACTIVE.pkts_received); */

            if (ACTIVE.pkts_received == pkts_received) {
                total_proc_time     += kvz_rtp::clock::hrc::diff_now_us(ACTIVE.start);
                total_frames_recved += 1;

                __resolve_relocations(frames, frames.active_ts);

                frames.total_received       += ACTIVE.pkts_received;
                frames.total_bytes_received += ACTIVE.frame->payload_len;
                frames.total_copied         += ACTIVE.relocs;

#if 0
                fprintf(stderr, "%zu vs %zu (%f) (%u MB)\n",
                        frames.total_received, frames.total_copied,
                        ((double)frames.total_copied / (double)frames.total_received) * 100,
                        frames.total_bytes_received / 1000 / 1000);
#endif

                ACTIVE.frame->payload_len = ACTIVE.received_size;
                reader->return_frame(ACTIVE.frame);
                frames.prev_eseq = ACTIVE.e_seq;
                frames.finfo.erase(frames.active_ts);
                change_active_frame = true;

#if 0
                fprintf(stderr, "total received %u | total dropped %u (%f% received)\n",K
                        total_pkts_received, total_pkts_dropped,
                        100 * (1 - ((double)total_pkts_dropped / (double)total_pkts_received)));
#endif
            }
        }

        if (change_active_frame) {
            /* Frames are resolved in order meaning that the oldest frame is resolved next */
            if (frames.tss.size() != 0) {
                frames.active_ts = __get_next_ts(frames.tss);
                ACTIVE.start     = kvz_rtp::clock::hrc::now();

                /* Resolve all relocations (if any)
                 * Record in relocation info vector doesn't necessarily mean that the
                 * fragment is in wrong place/its place cannot be determined
                 *
                 * Relocating at this point in the frame's lifetime is a delicate issue IF the fragments
                 * already received aren't contigous: TODO. selitä miksi ei voi välttämättä relokoida */
                __resolve_relocations(frames, frames.active_ts);
            } else {
                /* Previous (returned) framed was received so that "# of fragments" % MAX_DATAGRAMS == 0
                 *
                 * This means that we have no knowledge of the next frame and we must set the active_ts
                 * to INVALID_TS and deal with the timestamp relocation once we know better */
                __create_frame_entry(frames, INVALID_TS, NULL, 0);
                frames.active_ts = INVALID_TS;
            }

            ACTIVE.last_seq = INVALID_SEQ;
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
