#pragma once

#include <string>
#include <vector>

#include "media_stream.hh"
#include "zrtp.hh"

namespace kvz_rtp {

    class session {
        public:
            session(std::string addr);
            ~session();

            /* Create bidirectional media stream for media format "fmt"
             *
             * "flags" shall contain configuration information global to the media stream such as "RCE_SRTP"
             *
             * kvzRTP binds itself to "src_port" and sends packets to "addr":"dst_port" ("addr" given to constructor)
             *
             * Return pointer to media stream on success
             * Return nullptr if media stream allocation or initialization failed (more on initialization in media_stream.hh) */
            kvz_rtp::media_stream *create_stream(int src_port, int dst_port, rtp_format_t fmt, int flags);

            /* Destroy media_stream "stream"
             *
             * Return RTP_OK on success
             * Return RTP_INVALID_VALUE if "stream" is nullptr */
            rtp_error_t destroy_media_stream(kvz_rtp::media_stream *stream);

        private:
            /* Each RTP multimedia session shall have one ZRTP session from which all session are derived */
            kvz_rtp::zrtp zrtp_;

            /* Each RTP multimedia session is always IP-specific */
            std::string addr_;

            /* All media streams of this session */
            std::unordered_map<uint32_t, kvz_rtp::media_stream *> streams_;
    };
};
