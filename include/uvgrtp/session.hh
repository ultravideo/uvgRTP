#pragma once

#include "util.hh"

#include <mutex>
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>

namespace uvgrtp {

    class media_stream;
    class zrtp;

    class session {
        public:
            /// \cond DO_NOT_DOCUMENT
            session(std::string addr);
            session(std::string remote_addr, std::string local_addr);
            ~session();
            /// \endcond

            /**
             * \brief Create a bidirectional media stream for an RTP session
             *
             * \details
             *
             * If local_addr was provided when uvgrtp::session was created, uvgRTP binds
             * itself to local_addr:src_port, otherwise to INADDR_ANY:src_port
             *
             * This object is used for both sending and receiving media, see documentation
             * for uvgrtp::media_stream for more details.
             *
             * User can enable and disable functionality of uvgRTP by OR'ing RCE_* flags
             * together and passing them using the flags parameter
             *
             * \param src_port Local port that uvgRTP listens to for incoming RTP packets
             * \param dst_port Remote port where uvgRTP sends RTP packets
             * \param fmt      Format of the media stream. see ::RTP_FORMAT for more details
             * \param flags    RTP context enable flags, see ::RTP_CTX_ENABLE_FLAGS for more details
             *
             * \return RTP media stream object
             *
             * \retval uvgrtp::media_stream*  On success
             * \retval nullptr                 If src_port or dst_port is 0
             * \retval nullptr                 If fmt is not a supported media format
             * \retval nullptr                 If socket initialization failed
             * \retval nullptr                 If ZRTP was enabled and it failed to finish handshaking
             * \retval nullptr                 If RCE_SRTP is given but uvgRTP has not been compiled with Crypto++ enabled
             * \retval nullptr                 If RCE_SRTP is given but RCE_SRTP_KMNGMNT_* flag is not given
             * \retval nullptr                 If memory allocation failed
             */
            uvgrtp::media_stream *create_stream(int src_port, int dst_port, rtp_format_t fmt, int flags);

            /**
             * \brief Destroy a media stream
             *
             * \param stream Pointer to the media stream that should be destroyed
             *
             * \return RTP error code
             *
             * \retval RTP_OK             On success
             * \retval RTP_INVALID_VALUE  If stream is nullptr
             * \retval RTP_NOT_FOUND      If stream does not belong to this session
             */
            rtp_error_t destroy_stream(uvgrtp::media_stream *stream);

            /// \cond DO_NOT_DOCUMENT
            /* Get unique key of the session
             * Used by context to index sessions */
            std::string& get_key();
            /// \endcond

        private:
            /* Each RTP multimedia session shall have one ZRTP session from which all session are derived */
            std::shared_ptr<uvgrtp::zrtp> zrtp_;

            /* Each RTP multimedia session is always IP-specific */
            std::string addr_;

            /* If user so wishes, the session can be bound to a certain interface */
            std::string laddr_;

            /* All media streams of this session */
            std::unordered_map<uint32_t, uvgrtp::media_stream *> streams_;

            std::mutex session_mtx_;
    };
}

namespace uvg_rtp = uvgrtp;
