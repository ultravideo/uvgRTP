#pragma once

#include <string>
#include <vector>

#include "media_stream.hh"
#include "zrtp.hh"

namespace uvg_rtp {

    class session {
        public:
            session(std::string addr);
            session(std::string remote_addr, std::string local_addr);
            ~session();

            /* Create bidirectional media stream for media format "fmt"
             *
             * "flags" shall contain configuration information global to the media stream such as "RCE_SRTP"
             *
             * uvgRTP binds itself to "src_port" and sends packets to "addr":"dst_port" ("addr" given to constructor)
             *
             * Return pointer to media stream on success
             * Return nullptr if media stream allocation or initialization failed (more on initialization in media_stream.hh) */
            uvg_rtp::media_stream *create_stream(int src_port, int dst_port, rtp_format_t fmt, int flags);

            /* Destroy media_stream "stream"
             *
             * Return RTP_OK on success
             * Return RTP_INVALID_VALUE if "stream" is nullptr
             * Return RTP_NOT_FOUND if "stream" has not been allocated from this session */
            rtp_error_t destroy_stream(uvg_rtp::media_stream *stream);

            /* Get unique key of the session
             * Used by context to index sessions */
            std::string& get_key();

        private:
            /* Each RTP multimedia session shall have one ZRTP session from which all session are derived */
#ifdef __RTP_CRYPTO__
            uvg_rtp::zrtp *zrtp_;
#endif

            /* Each RTP multimedia session is always IP-specific */
            std::string addr_;

            /* If user so wishes, the session can be bound to a certain interface */
            std::string laddr_;

            /* All media streams of this session */
            std::unordered_map<uint32_t, uvg_rtp::media_stream *> streams_;
    };
};
