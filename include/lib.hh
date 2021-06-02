#pragma once

// these includes are here for easier usage of this library
// At this point I don't know which ones are required so I'll include them all
#include "formats/h26x.hh"
#include "formats/h264.hh"
#include "formats/h265.hh"
#include "formats/h266.hh"
#include "formats/media.hh"

#include "srtp/base.hh"
#include "srtp/srtcp.hh"
#include "srtp/srtp.hh"

#include "zrtp/commit.hh"
#include "zrtp/confack.hh"
#include "zrtp/confirm.hh"
#include "zrtp/defines.hh"
#include "zrtp/dh_kxchng.hh"
#include "zrtp/error.hh"
#include "zrtp/hello.hh"
#include "zrtp/hello_ack.hh"
#include "zrtp/zrtp_receiver.hh"

#include "clock.hh"
#include "crypto.hh"
#include "debug.hh"
#include "dispatch.hh"
#include "frame.hh"
#include "holepuncher.hh"
#include "hostname.hh"
#include "media_stream.hh"
#include "mingw_inet.hh"
#include "multicast.hh"
#include "pkt_dispatch.hh"
#include "poll.hh"
#include "queue.hh"
#include "random.hh"
#include "rtcp.hh"
#include "rtp.hh"
#include "runner.hh"
#include "session.hh"
#include "socket.hh"
#include "util.hh"
#include "zrtp.hh"

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
             * \retval nullptr               If "addr" is empty
             * \retval nullptr               If memory allocation failed
             */
            uvgrtp::session *create_session(std::string addr);

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
