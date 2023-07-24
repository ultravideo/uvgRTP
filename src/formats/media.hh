#pragma once

#include "uvgrtp/util.hh"

#include <map>
#include <memory>
#include <unordered_map>
#include <unordered_set>

#ifdef _WIN32
#include <ws2def.h>
#include <ws2ipdef.h>
#else
#include <netinet/in.h>
#endif

namespace uvgrtp {

    class socket;
    class rtp;
    class frame_queue;

    namespace frame {
        struct rtp_frame;
    }

    namespace formats {

        #define INVALID_TS            0xffffffff

        /* TODO: This functionality has much in common with h26x fragmentation and 
         * they could use same structures */ 

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
                media(std::shared_ptr<uvgrtp::socket> socket, std::shared_ptr<uvgrtp::rtp> rtp_ctx, int rce_flags);
                virtual ~media();

                /* These two functions are called by media_stream which is self is called by the application.
                 * They act as thunks and forward the call to push_media_frame() which every media should
                 * implement if they require more processing than what the default implementation offers
                 *
                 * Return RTP_OK on success */
                rtp_error_t push_frame(sockaddr_in& addr, sockaddr_in6& addr6, uint8_t *data, size_t data_len, int rtp_flags);
                rtp_error_t push_frame(sockaddr_in& addr, sockaddr_in6& addr6, std::unique_ptr<uint8_t[]> data, size_t data_len, int rtp_flags);

                /* Media-specific packet handler. The default handler, depending on what "rce_flags_" contains,
                 * may only return the received RTP packet or it may merge multiple packets together before
                 * returning a complete frame to the user.
                 *
                 * If the implemented media requires more fine-tuned processing, this should be overrode
                 *
                 * Return RTP_OK if the packet was successfully handled
                 * Return RTP_PKT_NOT_HANDLED if the packet is not handled by this handler
                 * Return RTP_PKT_MODIFIED if the packet was modified but should be forwarded to other handlers
                 * Return RTP_GENERIC_ERROR if the packet was corrupted in some way */
                rtp_error_t packet_handler(void* arg, int rce_flags, uint8_t* read_ptr, size_t size, frame::rtp_frame** out);

                /* Return pointer to the internal frame info structure which is relayed to packet handler */
                media_frame_info_t *get_media_frame_info();

                void set_fps(ssize_t enumarator, ssize_t denominator);

            protected:
                virtual rtp_error_t push_media_frame(sockaddr_in& addr, sockaddr_in6& addr6, uint8_t *data, size_t data_len, int rtp_flags);

                std::shared_ptr<uvgrtp::socket> socket_;
                std::shared_ptr<uvgrtp::rtp> rtp_ctx_;
                int rce_flags_;
                std::unique_ptr<uvgrtp::frame_queue> fqueue_;

            private:
                media_frame_info_t minfo_;
        };
    }
}

namespace uvg_rtp = uvgrtp;
