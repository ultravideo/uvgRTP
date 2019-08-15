#include <cstring>
#include <iostream>

#include "debug.hh"
#include "frame.hh"
#include "reader.hh"
#include "rtp_hevc.hh"
#include "rtp_opus.hh"

#define RTP_HEADER_VERSION  2

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

    switch (get_payload()) {
        case RTP_FORMAT_OPUS:
        case RTP_FORMAT_GENERIC:
            runner_ = new std::thread(kvz_rtp::generic::frame_receiver, this);
            break;

        case RTP_FORMAT_HEVC:
            runner_ = new std::thread(kvz_rtp::hevc::frame_receiver, this);
            break;
    }
    runner_->detach();

    if ((rtcp_ = new kvz_rtp::rtcp(get_ssrc(), true)) == nullptr) {
        LOG_ERROR("Failed to allocate RTCP instance!");
        return RTP_MEMORY_ERROR;
    }

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

void kvz_rtp::reader::return_frame(kvz_rtp::frame::rtp_frame *frame)
{
    if (recv_hook_installed())
        recv_hook(frame);
    else
        add_outgoing_frame(frame);
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

        if ((ssize_t)(frame->payload_len - frame->header.cc * sizeof(uint32_t)) < 0) {
            LOG_DEBUG("invalid frame length, %u CSRC entries, total length %u", frame->header.cc, frame->payload_len);
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
        frame->ext = new kvz_rtp::frame::ext_header;

        frame->ext->type = ntohs(*(uint16_t *)&ptr[0]);
        frame->ext->len  = ntohs(*(uint32_t *)&ptr[1]);
        frame->ext->data = (uint8_t *)ptr + 4;

        ptr += 2 * sizeof(uint16_t) + frame->ext->len;
    }

    /* If padding is set to 1, the last byte of the payload indicates
     * how many padding bytes was used. Make sure the padding length is
     * valid and subtract the amount of padding bytes from payload length */
    if (frame->header.padding) {
        LOG_DEBUG("frame contains padding");
        uint8_t padding_len = frame->payload[frame->payload_len - 1];

        if (padding_len == 0 || frame->payload_len <= padding_len) {
            rtp_errno = RTP_INVALID_VALUE;
            return nullptr;
        }

        frame->payload_len -= padding_len;
        frame->padding_len  = padding_len;
    }

    frame->payload = new uint8_t[frame->payload_len];
    std::memcpy(frame->payload, ptr, frame->payload_len);

    return frame;
}
