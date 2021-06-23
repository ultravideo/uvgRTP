#pragma once

#include "util.hh"

#include <map>
#include <memory>
#include <unordered_map>
#include <unordered_set>

namespace uvgrtp {

    class socket;
    class rtp;
    class frame_queue;

    namespace frame {
        struct rtp_frame;
    };

    namespace formats {

        #define INVALID_TS            0xffffffff

        typedef struct media_info {
            uint32_t s_seq = 0;
            uint32_t e_seq = 0;
            size_t npkts = 0;
            size_t size = 0;
            std::map<uint32_t, uvgrtp::frame::rtp_frame *> fragments;
        } media_info_t;

        typedef struct media_frame_info {
            std::unordered_map<uint32_t, media_info> frames;
            std::unordered_set<uint32_t> dropped;
        } media_frame_info_t;

        class media {
            public:
                media(uvgrtp::socket *socket, uvgrtp::rtp *rtp_ctx, int flags);
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

                /* Return pointer to the internal frame info structure which is relayed to packet handler */
                media_frame_info_t *get_media_frame_info();

            protected:
                virtual rtp_error_t push_media_frame(uint8_t *data, size_t data_len, int flags);

                uvgrtp::socket *socket_;
                uvgrtp::rtp *rtp_ctx_;
                int flags_;
                uvgrtp::frame_queue *fqueue_;

            private:
                media_frame_info_t minfo_;
        };
    };
};

namespace uvg_rtp = uvgrtp;
