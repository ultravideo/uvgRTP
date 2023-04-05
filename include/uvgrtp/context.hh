#pragma once

#include "util.hh"

#include <map>
#include <string>
#include <memory>


namespace uvgrtp {

    class session;
    class socketfactory;

    /**
     * \brief Provides CNAME isolation and can be used to create uvgrtp::session objects
     */
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
             * \param address IP address of the remote participant
             *
             * \return RTP session object
             *
             * \retval uvgrtp::session      On success
             * \retval nullptr               If "address" is empty or memory allocation failed
             */
            uvgrtp::session *create_session(std::string address);

            /**
             * \brief Create a new RTP session
             *
             * \details If UDP holepunching should be utilized, in addition to remote IP
             * address, the caller must also provide local IP address where uvgRTP
             * should bind itself to. If you are using uvgRTP for unidirectional streaming,
             * please take a look at @ref RCE_HOLEPUNCH_KEEPALIVE
             *
             * \param remote_addr IP address of the remote participant
             * \param local_addr  IP address of a local interface
             *
             * \return RTP session object
             *
             * \retval uvgrtp::session     On success
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

            /**
             * \brief Has Crypto++ been included in uvgRTP library
             *
             * \retval true      Crypto++ has been included, using SRTP is possible
             * \retval false     Crypto++ has not been included, using SRTP is not possible
             */
            bool crypto_enabled() const;

        private:
            /* Generate CNAME for participant using host and login names */
            std::string generate_cname() const;

            /* CNAME is the same for all connections */
            std::string cname_;
            std::shared_ptr<uvgrtp::socketfactory> sfp_;
        };
}

namespace uvg_rtp = uvgrtp;
