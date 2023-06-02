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
    class socketfactory;

    /** \brief Provides ZRTP synchronization and can be used to create uvgrtp::media_stream objects
     *
     * \details
     * 
     * 
     * 
     * By itself session does not do anything. The actual RTP streaming is done by media_stream objects, 
     * which can be created by session. media_stream corresponds to an RTP session in RFC 3550.
     */

    class session {
        public:
            /// \cond DO_NOT_DOCUMENT
            session(std::string cname, std::string addr, std::shared_ptr<uvgrtp::socketfactory> sfp);
            session(std::string cname, std::string remote_addr, std::string local_addr, std::shared_ptr<uvgrtp::socketfactory> sfp);
            ~session();
            /// \endcond

            /**
             * \brief Create a uni- or bidirectional media stream
             *
             * \details
             *
             * The created object is used for sending and/or receiving media, see documentation
             * for uvgrtp::media_stream for more details.
             *
             * If both addresses were provided when uvgrtp::session was created, by default 
             * uvgRTP binds itself to local_addr:src_port and sends packets to remote_addr:dst_port.
             * 
             * User can enable and disable functionality of media_stream by OR'ing (using |) RCE_* flags
             * together and passing them using the rce_flags parameter. In rce_flags, the RCE_SEND_ONLY flag 
             * can be used to avoid binding and src_port is thus ignored. Correspondinly RCE_RECEIVE_ONLY flag 
             * means dst_port is ignored. Without either RCE_SEND_ONLY or RCE_RECEIVE_ONLY, 
             * and if only one address was provided for session that address is interpreted as remote_addr and 
             * binding happens to ANY:src_port.
             *
             * \param src_port   Local port that uvgRTP listens to for incoming RTP packets
             * \param dst_port   Remote port where uvgRTP sends RTP packets
             * \param fmt        Format of the media stream. see ::RTP_FORMAT for more details
             * \param rce_flags  RTP context enable flags, see ::RTP_CTX_ENABLE_FLAGS for more details
             *
             * \return RTP media stream object
             *
             * \retval uvgrtp::media_stream*  On success
             * \retval nullptr                On failure, see print and 
             */
            uvgrtp::media_stream *create_stream(uint16_t src_port, uint16_t dst_port, rtp_format_t fmt, int rce_flags);

            /**
             * \brief Create a unidirectional media_stream for an RTP session
             *
             * \details
             *
             * The created object is used for sending or receiving media, see documentation
             * for uvgrtp::media_stream for more details.

             * User can enable and disable functionality of uvgRTP by OR'ing (using |) RCE_* flags
             * together and passing them using the rce_flags parameter. The RCE_SEND_ONLY flag in 
             * rce_flags means the port is interpreted as a remote port. The RCE_RECEIVE_ONLY means 
             * the port is used for binding to a local interface. Without either flag, 
             * this function defaults to RCE_SEND_ONLY.
             *
             * \param port         Either local or remote port depending on rce_flags
             * \param fmt          Format of the media stream. see ::RTP_FORMAT for more details
             * \param rce_flags    RTP context enable flags, see ::RTP_CTX_ENABLE_FLAGS for more details
             *
             * \return RTP media stream object
             *
             * \retval uvgrtp::media_stream*  On success
             * \retval nullptr                On failure, see print
             */
            uvgrtp::media_stream *create_stream(uint16_t port, rtp_format_t fmt, int rce_flags);

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

            std::string generic_address_;

            /* Each RTP multimedia session is always IP-specific */
            std::string remote_address_;

            /* If user so wishes, the session can be bound to a certain interface */
            std::string local_address_;

            /* All media streams of this session */
            std::unordered_map<uint32_t, uvgrtp::media_stream *> streams_;

            std::mutex session_mtx_;

            std::string cname_;
            std::shared_ptr<uvgrtp::socketfactory> sf_;
    };
}

namespace uvg_rtp = uvgrtp;
