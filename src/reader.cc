#include <cstring>
#include <iostream>

#include "debug.hh"
#include "frame.hh"
#include "reader.hh"
#include "rtp_hevc.hh"
#include "rtp_opus.hh"

#define RETURN_FRAME(frame) \
    do { \
        if (reader->recv_hook_installed()) \
            reader->recv_hook(frame); \
        else  \
            reader->add_outgoing_frame(frame); \
    } while (0);


#define RTP_HEADER_VERSION             2
#define RTP_FRAME_MAX_DELAY           50
#define INVALID_SEQ           0x13371338

struct hevc_fu_info {
    kvz_rtp::clock::hrc::hrc_t sframe_time; /* clock reading when the first fragment is received */
    uint32_t sframe_seq;                    /* sequence number of the frame with s-bit */
    uint32_t eframe_seq;                    /* sequence number of the frame with e-bit */
    size_t pkts_received;                   /* how many fragments have been received */
    size_t total_size;                      /* total size of all fragments */
};

kvz_rtp::reader::reader(std::string src_addr, int src_port):
    connection(true),
    active_(false),
    src_addr_(src_addr),
    src_port_(src_port),
    recv_hook_arg_(nullptr),
    recv_hook_(nullptr)
{
}

kvz_rtp::reader::~reader()
{
    active_ = false;
    delete[] recv_buffer_;

    if (!framesOut_.empty()) {
        for (auto &i : framesOut_) {
            if (kvz_rtp::frame::dealloc_frame(i) != RTP_OK) {
                LOG_ERROR("Failed to deallocate frame!");
            }
        }

        framesOut_.clear();
    }
}

rtp_error_t kvz_rtp::reader::start()
{
    rtp_error_t ret;

    if ((ret = socket_.init(AF_INET, SOCK_DGRAM, 0)) != RTP_OK)
        return ret;

    int enable       = 1;
    int udp_buf_size = 0xffff;

    if ((ret = socket_.setsockopt(SOL_SOCKET, SO_REUSEADDR, (const char *)&enable, sizeof(int))) != RTP_OK)
        return ret;

    if ((ret = socket_.setsockopt(SOL_SOCKET, SO_RCVBUF, (const char *)&udp_buf_size, sizeof(int))) != RTP_OK)
        return ret;

    LOG_DEBUG("Binding to port %d (source port)", src_port_);

    if ((ret = socket_.bind(AF_INET, INADDR_ANY, src_port_)) != RTP_OK)
        return ret;

    recv_buffer_len_ = 4096;

    if ((recv_buffer_ = new uint8_t[4096]) == nullptr) {
        LOG_ERROR("Failed to allocate buffer for incoming data!");
        recv_buffer_len_ = 0;
    }

    active_ = true;

    runner_ = new std::thread(frame_receiver, this);
    runner_->detach();

    return RTP_OK;
}

kvz_rtp::frame::rtp_frame *kvz_rtp::reader::pull_frame()
{
    while (framesOut_.empty() && this->active()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    if (!this->active())
        return nullptr;

    frames_mtx_.lock();
    auto nextFrame = framesOut_.front();
    framesOut_.erase(framesOut_.begin());
    frames_mtx_.unlock();

    return nextFrame;
}

bool kvz_rtp::reader::active()
{
    return active_;
}

uint8_t *kvz_rtp::reader::get_recv_buffer() const
{
    return recv_buffer_;
}

uint32_t kvz_rtp::reader::get_recv_buffer_len() const
{
    return recv_buffer_len_;
}

void kvz_rtp::reader::add_outgoing_frame(kvz_rtp::frame::rtp_frame *frame)
{
    if (!frame)
        return;

    framesOut_.push_back(frame);
}

bool kvz_rtp::reader::recv_hook_installed()
{
    return recv_hook_ != nullptr;
}

void kvz_rtp::reader::install_recv_hook(void *arg, void (*hook)(void *arg, kvz_rtp::frame::rtp_frame *))
{
    if (hook == nullptr) {
        LOG_ERROR("Unable to install receive hook, function pointer is nullptr!");
        return;
    }

    recv_hook_     = hook;
    recv_hook_arg_ = arg;
}

void kvz_rtp::reader::recv_hook(kvz_rtp::frame::rtp_frame *frame)
{
    if (recv_hook_)
        return recv_hook_(recv_hook_arg_, frame);
}

void kvz_rtp::reader::return_frame(kvz_rtp::reader *reader, kvz_rtp::frame::rtp_frame *frame)
{
    if (reader->recv_hook_installed())
        reader->recv_hook(frame);
    else
        reader->add_outgoing_frame(frame);
}

rtp_error_t kvz_rtp::reader::read_rtp_header(kvz_rtp::frame::rtp_header *dst, uint8_t *src)
{
    if (!dst || !src)
        return RTP_INVALID_VALUE;

    dst->version   = (src[0] >> 6) & 0x03;
    dst->padding   = (src[0] >> 5) & 0x01;
    dst->ext       = (src[0] >> 4) & 0x01;
    dst->cc        = (src[0] >> 0) & 0x0f;
    dst->marker    = (src[1] & 0x80) ? 1 : 0;
    dst->payload   = (src[1] & 0x7f);
    dst->seq       = ntohs(*(uint16_t *)&src[2]);
    dst->timestamp = ntohl(*(uint32_t *)&src[4]);
    dst->ssrc      = ntohl(*(uint32_t *)&src[8]);

    return RTP_OK;
}

kvz_rtp::frame::rtp_frame *kvz_rtp::reader::validate_rtp_frame(uint8_t *buffer, int size)
{
    if (!buffer || size < 12) {
        rtp_errno = RTP_INVALID_VALUE;
        return nullptr;
    }

    uint8_t *ptr                     = buffer;
    kvz_rtp::frame::rtp_frame *frame = kvz_rtp::frame::alloc_rtp_frame();

    if (!frame) {
        LOG_ERROR("failed to allocate memory for RTP frame");
        return nullptr;
    }

    if (kvz_rtp::reader::read_rtp_header(&frame->header, buffer) != RTP_OK) {
        LOG_ERROR("failed to read the RTP header");
        return nullptr;
    }

    frame->total_len   = (size_t)size;
    frame->payload_len = (size_t)size - sizeof(kvz_rtp::frame::rtp_header);

    if (frame->header.version != RTP_HEADER_VERSION) {
        LOG_ERROR("inavlid version");
        rtp_errno = RTP_INVALID_VALUE;
        return nullptr;
    }

    if (frame->header.marker) {
        LOG_DEBUG("header has marker set");
    }

    /* Skip the generic RTP header
     * There may be 0..N CSRC entries after the header, so check those
     * After CSRC there may be extension header */
    ptr += sizeof(kvz_rtp::frame::rtp_header);

    if (frame->header.cc > 0) {
        LOG_DEBUG("frame contains csrc entries");

        if ((ssize_t)(frame->total_len - sizeof(kvz_rtp::frame::rtp_header) - frame->header.cc * sizeof(uint32_t)) < 0) {
            LOG_DEBUG("invalid frame length, %u CSRC entries, total length %u", frame->header.cc, frame->total_len);
            rtp_errno = RTP_INVALID_VALUE;
            return nullptr;
        }
        LOG_DEBUG("Allocating %u CSRC entries", frame->header.cc);

        frame->csrc         = new uint32_t[frame->header.cc];
        frame->payload_len -= frame->header.cc * sizeof(uint32_t);

        for (size_t i = 0; i < frame->header.cc; ++i) {
            frame->csrc[i] = *(uint32_t *)ptr;
            ptr += sizeof(uint32_t);
        }
    }

    if (frame->header.ext) {
        LOG_DEBUG("frame contains extension information");
        /* TODO: handle extension */
    }

    /* If padding is set to 1, the last byte of the payload indicates
     * how many padding bytes was used. Make sure the padding length is
     * valid and subtract the amount of padding bytes from payload length */
    if (frame->header.padding) {
        LOG_DEBUG("frame contains padding");
        uint8_t padding_len = frame->data[frame->total_len - 1];

        if (padding_len == 0 || frame->payload_len <= padding_len) {
            rtp_errno = RTP_INVALID_VALUE;
            return nullptr;
        }

        frame->payload_len -= padding_len;
        frame->padding_len  = padding_len;
    }

    frame->data = new uint8_t[frame->total_len];
    std::memcpy(frame->data, buffer, frame->total_len);
    frame->payload = frame->data + (frame->total_len - frame->payload_len);

    return frame;
}

void kvz_rtp::reader::frame_receiver(kvz_rtp::reader *reader)
{
    LOG_INFO("frameReceiver starting listening...");

    /* TODO: this looks super ugly */
    rtp_error_t ret;
    int nread = 0, ftype = 0;
    sockaddr_in sender_addr;
    kvz_rtp::socket socket = reader->get_socket();
    std::pair<size_t, std::vector<kvz_rtp::frame::rtp_frame *>> fu;
    kvz_rtp::frame::rtp_frame *frame, *f, *frames[0xffff + 1] = { 0 };

    uint8_t nal_header[2] = { 0 };
    std::map<uint32_t, hevc_fu_info> s_timers;
    std::map<uint32_t, size_t> dropped_frames;
    size_t n_dropped_frames = 0, n_new_frames = 0, cached = 0;

    while (reader->active()) {
        ret = socket.recvfrom(reader->get_recv_buffer(), reader->get_recv_buffer_len(), 0, &sender_addr, &nread);

        if (ret != RTP_OK) {
            LOG_ERROR("recvfrom failed! FrameReceiver cannot continue!");
            return;
        }

        if ((frame = validate_rtp_frame(reader->get_recv_buffer(), nread)) == nullptr) {
            LOG_DEBUG("received an invalid frame, discarding");
            continue;
        }

        /* Update session related statistics
         * If this is a new peer, RTCP will take care of initializing necessary stuff */
        reader->update_receiver_stats(frame);

        /* How to the frame is handled is based what its type is. Generic and Opus frames
         * don't require any extra processing so they can be returned to the user as soon as
         * they're received without any buffering.
         *
         * Frames that can be fragmented (only HEVC for now) require some preprocessing before they're returned.
         *
         * When a frame with an S-bit set is received, the frame is saved to "frames" array and an NTP timestamp
         * is saved. All subsequent frag frames are also saved in the "frames" array and they may arrive in
         * any order they want because they're saved in the array using the sequence number.
         *
         * Each time a new frame is received, the initial timestamp is compared with current time to see
         * how much time this frame still has left until it's discarded. Each frame is given N milliseconds
         * and if all its fragments are not received within that time window, the frame is considered invalid
         * and all fragments are discarded.
         *
         * When the last fragment is received (ie. the frame with E-bit set) AND if all previous fragments
         * very received too, frame_receiver() will call post-processing function to merge all the fragments
         * into one complete frame.
         *
         * If all previous fragments have not been received (ie. some frame is late), the code will wait
         * until the time windows closes. When that happens, the array is inspected once more to see if
         * all fragments were received and if so, the fragments are merged and returned to user.
         *
         * If some fragments were dropped, the whole HEVC frame is discarded
         *
         * Due to the nature of UDP, it's possible that during a fragment reception,
         * a stray RTP packet from earlier fragment might be received and that might corrupt
         * the reception process. To mitigate this, the sequence number and RTP timestamp of each
         * incoming packet is matched and checked against our own clock to get sense whether this packet valid
         *
         * Invalid packets (such as very late packets) are discarded automatically without further processing */
        switch (frame->header.payload) {
            case RTP_FORMAT_OPUS:
            case RTP_FORMAT_GENERIC:
                RETURN_FRAME(frame);
                break;

            /* TODO: should this be moved to a separate function? */
            case RTP_FORMAT_HEVC:
            {
                const size_t HEVC_HDR_SIZE =
                    kvz_rtp::frame::HEADER_SIZE_HEVC_NAL +
                    kvz_rtp::frame::HEADER_SIZE_HEVC_FU;

                int type = kvz_rtp::hevc::check_frame(frame);

                if (type == kvz_rtp::hevc::FT_NOT_FRAG) {
                    RETURN_FRAME(frame);
                    continue;
                }

                if (type == kvz_rtp::hevc::FT_INVALID) {
                    LOG_WARN("invalid frame received!");
                    break;
                }

                /* TODO: this is ugly */
                bool duplicate = true;

                /* Save the received frame to "frames" array where frames are indexed using
                 * their sequence number. This way when all fragments of a frame are received,
                 * we can loop through the range sframe_seq - eframe_seq and merge all fragments */
                if (frames[frame->header.seq] == nullptr) {
                    frames[frame->header.seq] = frame;
                    duplicate                 = false;
                }

                /* If this is the first packet received with this timestamp, create new entry
                 * to s_timers and save current time.
                 *
                 * This timestamp is used to keep track of how long we've been receiving chunks
                 * and if the time exceeds RTP_FRAME_MAX_DELAY, we drop the frame */
                if (s_timers.find(frame->header.timestamp) == s_timers.end()) {
                    n_new_frames++;

                    /* UDP being unreliable, we can't know for sure in what order the packets are arriving.
                     * Especially on linux where the fragment frames are batched and sent together it possible
                     * that the first fragment we receive is the fragment containing the E-bit which sounds weird
                     *
                     * When the first fragment is received (regardless of its type), the timer is started and if the
                     * fragment is special (S or E bit set), the sequence number is saved so we know the range of complete
                     * full HEVC frame if/when all fragments have been received */

                    if (type == kvz_rtp::hevc::FT_START) {
                        s_timers[frame->header.timestamp].sframe_seq = frame->header.seq;
                        s_timers[frame->header.timestamp].eframe_seq = INVALID_SEQ;
                    } else if (type == kvz_rtp::hevc::FT_END) {
                        s_timers[frame->header.timestamp].eframe_seq = frame->header.seq;
                        s_timers[frame->header.timestamp].sframe_seq = INVALID_SEQ;
                    } else {
                        s_timers[frame->header.timestamp].sframe_seq = INVALID_SEQ;
                        s_timers[frame->header.timestamp].eframe_seq = INVALID_SEQ;
                    }

                    s_timers[frame->header.timestamp].sframe_time   = kvz_rtp::clock::hrc::now();
                    s_timers[frame->header.timestamp].total_size    = frame->payload_len - HEVC_HDR_SIZE;
                    s_timers[frame->header.timestamp].pkts_received = 1;
                    continue;
                }

                uint64_t diff = kvz_rtp::clock::hrc::diff_now(s_timers[frame->header.timestamp].sframe_time);

                if (diff > RTP_FRAME_MAX_DELAY) {
                    if (dropped_frames.find(frame->header.timestamp) == dropped_frames.end()) {
                        dropped_frames[frame->header.timestamp] = 1;
                        LOG_ERROR("frame dropped %u, diff %u!", ++n_dropped_frames, diff);
                        kvz_rtp::frame::dealloc_frame(frame);
                    } else {
                        dropped_frames[frame->header.timestamp]++;
                        kvz_rtp::frame::dealloc_frame(frame);
                        /* TODO: do something? */
                    }
                    continue;
                }

                if (!duplicate) {
                    s_timers[frame->header.timestamp].pkts_received++;
                    s_timers[frame->header.timestamp].total_size += (frame->payload_len - HEVC_HDR_SIZE);
                }

                if (type == kvz_rtp::hevc::FT_START)
                    s_timers[frame->header.timestamp].sframe_seq = frame->header.seq;

                if (type == kvz_rtp::hevc::FT_END)
                    s_timers[frame->header.timestamp].eframe_seq = frame->header.seq;

                if (s_timers[frame->header.timestamp].sframe_seq != INVALID_SEQ &&
                    s_timers[frame->header.timestamp].eframe_seq != INVALID_SEQ)
                {
                    uint32_t ts    = frame->header.timestamp;
                    uint16_t s_seq = s_timers[ts].sframe_seq;
                    uint16_t e_seq = s_timers[ts].eframe_seq;
                    size_t ptr     = 0;

                    /* we've received every fragment and the frame can be reconstructed */
                    if (e_seq - s_seq + 1 == s_timers[frame->header.timestamp].pkts_received) {
                        nal_header[0] = (frames[s_seq]->payload[0] & 0x81) | ((frame->payload[2] & 0x3f) << 1);
                        nal_header[1] =  frames[s_seq]->payload[1];

                        kvz_rtp::frame::rtp_frame *out = kvz_rtp::frame::alloc_rtp_frame();

                        out->payload_len = s_timers[frame->header.timestamp].total_size + kvz_rtp::frame::HEADER_SIZE_HEVC_NAL;
                        out->payload     = new uint8_t[out->payload_len];
                        out->data        = out->payload;

                        std::memcpy(&out->header,  &frames[s_seq]->header, kvz_rtp::frame::HEADER_SIZE_RTP);
                        std::memcpy(out->payload,  nal_header,             kvz_rtp::frame::HEADER_SIZE_HEVC_NAL);

                        ptr += kvz_rtp::frame::HEADER_SIZE_HEVC_NAL;

                        for (size_t i = s_seq; i <= e_seq; ++i) {
                            std::memcpy(
                                &out->payload[ptr],
                                &frames[i]->payload[HEVC_HDR_SIZE],
                                frames[i]->payload_len - HEVC_HDR_SIZE
                            );
                            ptr += frames[i]->payload_len - HEVC_HDR_SIZE;
                            kvz_rtp::frame::dealloc_frame(frames[i]);
                        }

                        RETURN_FRAME(out);
                        s_timers.erase(ts);
                    }
                }
            }
            break;
        }
    }
}
