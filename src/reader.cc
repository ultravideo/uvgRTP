#include <cstring>
#include <iostream>

#include "debug.hh"
#include "frame.hh"
#include "reader.hh"
#include "rtp_hevc.hh"
#include "rtp_opus.hh"

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
    delete recv_buffer_;

    if (!framesOut_.empty()) {
        for (auto &i : framesOut_) {
            if (kvz_rtp::frame::dealloc_frame(i) != RTP_OK) {
                LOG_ERROR("Failed to deallocate frame!");
            }
        }
    }
}

rtp_error_t kvz_rtp::reader::start()
{
    rtp_error_t ret;

    if ((ret = socket_.init(AF_INET, SOCK_DGRAM, 0)) != RTP_OK)
        return ret;

    int enable = 1;

    if ((ret = socket_.setsockopt(SOL_SOCKET, SO_REUSEADDR, (const char *)&enable, sizeof(int))) != RTP_OK)
        return ret;

    LOG_DEBUG("Binding to port %d (source port)", src_port_);

    if ((ret = socket_.bind(AF_INET, INADDR_ANY, src_port_)) != RTP_OK)
        return ret;

    recv_buffer_len_ = MAX_PACKET;

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

void kvz_rtp::reader::frame_receiver(kvz_rtp::reader *reader)
{
    LOG_INFO("frameReceiver starting listening...");

    int nread = 0;
    rtp_error_t ret;
    sockaddr_in sender_addr;
    kvz_rtp::socket socket = reader->get_socket();
    std::pair<size_t, std::vector<kvz_rtp::frame::rtp_frame *>> fu;

    while (reader->active()) {
        ret = socket.recvfrom(reader->get_recv_buffer(), reader->get_recv_buffer_len(), 0, &sender_addr, &nread);

        if (ret != RTP_OK) {
            LOG_ERROR("recvfrom failed! FrameReceiver cannot continue!");
            return;
        }

        uint8_t *inbuf = reader->get_recv_buffer();
        auto *frame    = kvz_rtp::frame::alloc_rtp_frame(nread, kvz_rtp::frame::FRAME_TYPE_GENERIC);

        if (!frame) {
            LOG_ERROR("Failed to allocate RTP Frame!");
            continue;
        }

        frame->marker    = (inbuf[1] & 0x80) ? 1 : 0;
        frame->ptype     = (inbuf[1] & 0x7f);
        frame->seq       = ntohs(*(uint16_t *)&inbuf[2]);
        frame->timestamp = ntohl(*(uint32_t *)&inbuf[4]);
        frame->ssrc      = ntohl(*(uint32_t *)&inbuf[8]);

        if (nread - kvz_rtp::frame::HEADER_SIZE_RTP <= 0) {
            LOG_WARN("Got an invalid payload of size %d", nread);
            continue;
        }

        frame->data        = new uint8_t[nread];
        frame->payload     = frame->data + kvz_rtp::frame::HEADER_SIZE_RTP;
        frame->payload_len = nread - kvz_rtp::frame::HEADER_SIZE_RTP;
        frame->total_len   = nread;

        /* Update session related statistics
         *
         * If this is a new peer,
         * RTCP will take care of initializing necessary stuff */
        reader->update_receiver_stats(frame);

        if (!frame->data) {
            LOG_ERROR("Failed to allocate buffer for RTP frame!");
            continue;
        }

        memcpy(frame->data, inbuf, nread);

        switch (frame->ptype) {
            case RTP_FORMAT_OPUS:
                frame = kvz_rtp::opus::process_opus_frame(frame, fu, ret);
                break;

            case RTP_FORMAT_HEVC:
                frame = kvz_rtp::hevc::process_hevc_frame(frame, fu, ret);
                break;

            case RTP_FORMAT_GENERIC:
                frame = kvz_rtp::generic::process_generic_frame(frame, fu, ret);
                break;

            default:
                LOG_WARN("Unrecognized RTP payload type %u", frame->ptype);
                ret = RTP_INVALID_VALUE;
                break;
        }

        if (ret == RTP_OK) {
            LOG_DEBUG("returning frame!");

            if (reader->recv_hook_installed())
                reader->recv_hook(frame);
            else
                reader->add_outgoing_frame(frame);
        } else if (ret == RTP_NOT_READY) {
            LOG_DEBUG("received a fragmentation unit, unable return frame to user");
        } else {
            LOG_ERROR("Failed to process frame, error: %d", ret);
        }
    }

    LOG_INFO("FrameReceiver thread exiting...");
}
