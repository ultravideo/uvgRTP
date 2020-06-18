#include <cstring>
#include <iostream>
#include <thread>

#include "debug.hh"
#include "frame.hh"
#include "receiver.hh"
#include "rtcp.hh"

#include "formats/hevc.hh"
#include "formats/opus.hh"

#define RTP_HEADER_VERSION  2

uvg_rtp::receiver::receiver(uvg_rtp::socket& socket, rtp_ctx_conf& conf, rtp_format_t fmt, uvg_rtp::rtp *rtp):
    socket_(socket),
    rtcp_(nullptr),
    rtp_(rtp),
    conf_(conf),
    fmt_(fmt),
    recv_hook_(nullptr)
{
}

uvg_rtp::receiver::~receiver()
{
    delete[] recv_buf_;
}

rtp_error_t uvg_rtp::receiver::stop()
{
    r_mtx_.lock();
    active_ = false;

    while (!r_mtx_.try_lock())
        ;

    return RTP_OK;
}

rtp_error_t uvg_rtp::receiver::start()
{
    rtp_error_t ret  = RTP_OK;
    ssize_t buf_size = 4 * 1000 * 1000;

    if ((ret = socket_.setsockopt(SOL_SOCKET, SO_RCVBUF, (const char *)&buf_size, sizeof(int))) != RTP_OK)
        return ret;

    recv_buf_len_ = 4096;

    if ((recv_buf_ = new uint8_t[4096]) == nullptr) {
        LOG_ERROR("Failed to allocate buffer for incoming data!");
        recv_buf_len_ = 0;
    }
    active_ = true;

    switch (fmt_) {
        case RTP_FORMAT_OPUS:
        case RTP_FORMAT_GENERIC:
            runner_ = new std::thread(uvg_rtp::generic::frame_receiver, this);
            break;

        case RTP_FORMAT_HEVC:
            runner_ = new std::thread(uvg_rtp::hevc::frame_receiver, this, !!(conf_.flags & RCE_OPTIMISTIC_RECEIVER));
            break;
    }
    runner_->detach();

    return RTP_OK;
}

uvg_rtp::frame::rtp_frame *uvg_rtp::receiver::pull_frame()
{
    while (frames_.empty() && this->active()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    if (!this->active())
        return nullptr;

    frames_mtx_.lock();
    auto frame = frames_.front();
    frames_.erase(frames_.begin());
    frames_mtx_.unlock();

    return frame;
}

uvg_rtp::frame::rtp_frame *uvg_rtp::receiver::pull_frame(size_t timeout)
{
    while (frames_.empty() && this->active() && timeout) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        --timeout;
    }

    if (!this->active())
        return nullptr;

    if (frames_.empty())
        return nullptr;

    frames_mtx_.lock();
    auto frame = frames_.front();
    frames_.erase(frames_.begin());
    frames_mtx_.unlock();

    return frame;
}

uint8_t *uvg_rtp::receiver::get_recv_buffer() const
{
    return recv_buf_;
}

uint32_t uvg_rtp::receiver::get_recv_buffer_len() const
{
    return recv_buf_len_;
}

void uvg_rtp::receiver::add_outgoing_frame(uvg_rtp::frame::rtp_frame *frame)
{
    if (!frame)
        return;

    frames_.push_back(frame);
}

bool uvg_rtp::receiver::recv_hook_installed()
{
    return recv_hook_ != nullptr;
}

void uvg_rtp::receiver::install_recv_hook(void *arg, void (*hook)(void *arg, uvg_rtp::frame::rtp_frame *))
{
    if (hook == nullptr) {
        LOG_ERROR("Unable to install receive hook, function pointer is nullptr!");
        return;
    }

    recv_hook_     = hook;
    recv_hook_arg_ = arg;
}

void uvg_rtp::receiver::install_notify_hook(void *arg, void (*hook)(void *arg, int notify))
{
    if (hook == nullptr) {
        LOG_ERROR("Unable to install receive hook, function pointer is nullptr!");
        return;
    }

    notify_hook_     = hook;
    notify_hook_arg_ = arg;
}

void uvg_rtp::receiver::recv_hook(uvg_rtp::frame::rtp_frame *frame)
{
    if (recv_hook_)
        return recv_hook_(recv_hook_arg_, frame);
}

void uvg_rtp::receiver::return_frame(uvg_rtp::frame::rtp_frame *frame)
{
    if (recv_hook_installed())
        recv_hook(frame);
    else
        add_outgoing_frame(frame);
}

rtp_error_t uvg_rtp::receiver::read_rtp_header(uvg_rtp::frame::rtp_header *dst, uint8_t *src)
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

uvg_rtp::frame::rtp_frame *uvg_rtp::receiver::validate_rtp_frame(uint8_t *buffer, int size)
{
    if (!buffer || size < 12) {
        rtp_errno = RTP_INVALID_VALUE;
        return nullptr;
    }

    uint8_t *ptr                     = buffer;
    uvg_rtp::frame::rtp_frame *frame = uvg_rtp::frame::alloc_rtp_frame();

    if (!frame) {
        LOG_ERROR("failed to allocate memory for RTP frame");
        return nullptr;
    }

    if (uvg_rtp::receiver::read_rtp_header(&frame->header, buffer) != RTP_OK) {
        LOG_ERROR("failed to read the RTP header");
        return nullptr;
    }

    frame->payload_len = (size_t)size - sizeof(uvg_rtp::frame::rtp_header);

    if (frame->header.version != RTP_HEADER_VERSION) {

        /* TODO: zrtp packet should not be ignored */
        if (frame->header.version == 0 && (conf_.flags & RCE_SRTP_KMNGMNT_ZRTP)) {
            rtp_errno = RTP_OK;
            return nullptr;
        }

        LOG_ERROR("inavlid version %d", frame->header.version);
        rtp_errno = RTP_INVALID_VALUE;
        return nullptr;
    }

    if (frame->header.marker) {
        LOG_DEBUG("header has marker set");
    }

    /* Skip the generic RTP header
     * There may be 0..N CSRC entries after the header, so check those
     * After CSRC there may be extension header */
    ptr += sizeof(uvg_rtp::frame::rtp_header);

    if (frame->header.cc > 0) {
        LOG_DEBUG("frame contains csrc entries");

        if ((ssize_t)(frame->payload_len - frame->header.cc * sizeof(uint32_t)) < 0) {
            LOG_DEBUG("invalid frame length, %d CSRC entries, total length %zu", frame->header.cc, frame->payload_len);
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
        frame->ext = new uvg_rtp::frame::ext_header;

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

rtp_error_t uvg_rtp::receiver::update_receiver_stats(uvg_rtp::frame::rtp_frame *frame)
{
    rtp_error_t ret;

    if (rtcp_) {
        if ((ret = rtcp_->receiver_update_stats(frame)) != RTP_SSRC_COLLISION)
            return ret;

        /* TODO: fix ssrc collisions */
        /* do { */
            /* rtp_ssrc_ = kvz_rtp::random::generate_32(); */
        /* } while ((rtcp_->reset_rtcp_state(rtp_ssrc_)) != RTP_OK); */

        /* even though we've resolved the SSRC conflict, we still need to return an error
         * code because the original packet that caused the conflict is considered "invalid" */
        return RTP_INVALID_VALUE;
    }

    return RTP_OK;

}

uvg_rtp::socket& uvg_rtp::receiver::get_socket()
{
    return socket_;
}

uvg_rtp::rtp *uvg_rtp::receiver::get_rtp_ctx()
{
    return rtp_;
}

std::mutex& uvg_rtp::receiver::get_mutex()
{
    return r_mtx_;
}

rtp_ctx_conf& uvg_rtp::receiver::get_conf()
{
    return conf_;
}

void uvg_rtp::receiver::set_rtcp(uvg_rtp::rtcp *rtcp)
{
    rtcp_ = rtcp;
}
