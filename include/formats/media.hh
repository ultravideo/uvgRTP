#pragma once

#include <memory>

#include "../rtp.hh"
#include "../socket.hh"
#include "../queue.hh"
#include "../util.hh"

namespace uvg_rtp {

    namespace formats {

        class media {
            public:
                media(uvg_rtp::socket *socket, uvg_rtp::rtp *rtp_ctx, int flags);
                virtual ~media();

                /* These two functions are called by media_stream which is self is called by the application.
                 * They act as thunks and forward the call to push_media_frame() which every media should
                 * implement if they require more processing than what the default implementation offers
                 *
                 * Return RTP_OK on success */
                rtp_error_t push_frame(uint8_t *data, size_t data_len, int flags);
                rtp_error_t push_frame(std::unique_ptr<uint8_t[]> data, size_t data_len, int flags);

                /* Media-specific packet handler. The default handler, depending on what "flags_" contains,
                 * may only return the received RTP packet or it may merge multiple packets together before
                 * returning a complete frame to the user.
                 *
                 * If the implemented media requires more fine-tuned processing, this should be overrode
                 *
                 * Return RTP_OK if the packet was successfully handled
                 * Return RTP_PKT_NOT_HANDLED if the packet is not handled by this handler
                 * Return RTP_PKT_MODIFIED if the packet was modified but should be forwarded to other handlers
                 * Return RTP_GENERIC_ERROR if the packet was corrupted in some way */
                static rtp_error_t packet_handler(void *arg, int flags, frame::rtp_frame **frame);

            protected:
                virtual rtp_error_t push_media_frame(uint8_t *data, size_t data_len, int flags);

                uvg_rtp::socket *socket_;
                uvg_rtp::rtp *rtp_ctx_;
                int flags_;
                uvg_rtp::frame_queue *fqueue_;
        };
    };
};
