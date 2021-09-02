#pragma once

// these includes are here for easier usage of this library
#include "media_stream.hh"  // media streamer class
#include "session.hh"       // session class
#include "rtcp.hh"          // RTCP
#include "clock.hh"         // time related functions
#include "crypto.hh"        // check if crypto is enabled
#include "debug.hh"         // debug prints
#include "frame.hh"         // frame related functions
#include "util.hh"          // types
#include "version.hh"

#include <map>
#include <string>

namespace uvgrtp {

    class context {
        public:
            /**
             * \brief RTP context constructor
             *
             * \details Most of the time one RTP context per application is enough.
             * If CNAME namespace isolation is required, multiple context objects can be created.
             */
            context();

            /**
             * \brief RTP context destructor
             *
             * \details This does not destroy active sessions. They must be destroyed manually
             * by calling uvgrtp::context::destroy_session()
             */
            ~context();

            /**
             * \brief Create a new RTP session
             *
             * \param addr IPv4 address of the remote participant
             *
             * \return RTP session object
             *
             * \retval uvgrtp::session      On success
             * \retval nullptr               If "remote_addr" is empty
             * \retval nullptr               If memory allocation failed
             */
            uvgrtp::session *create_session(std::string remote_addr);

            /**
             * \brief Create a new RTP session
             *
             * \details If UDP holepunching should be utilized, in addition to remote IP
             * address, the caller must also provide local IP address where uvgRTP
             * should bind itself to. If you are using uvgRTP for unidirectional streaming,
             * please take a look at @ref RCE_HOLEPUNCH_KEEPALIVE
             *
             * \param remote_addr IPv4 address of the remote participant
             * \param local_addr  IPv4 address of a local interface
             *
             * \return RTP session object
             *
             * \retval uvgrtp::session     On success
             * \retval nullptr              If remote_addr or local_addr is empty
             * \retval nullptr              If memory allocation failed
             */
            uvgrtp::session *create_session(std::string remote_addr, std::string local_addr);

            /**
             * \brief Destroy RTP session and all of its media streams
             *
             * \param session Pointer to the session object that should be destroyed
             *
             * \return RTP error code
             *
             * \retval RTP_OK                On success
             * \retval RTP_INVALID_VALUE     If session is nullptr
             */
            rtp_error_t destroy_session(uvgrtp::session *session);

            /// \cond DO_NOT_DOCUMENT
            std::string& get_cname();
            /// \endcond

        private:
            /* Generate CNAME for participant using host and login names */
            std::string generate_cname();

            /* CNAME is the same for all connections */
            std::string cname_;
        };
};

namespace uvg_rtp = uvgrtp;
