#pragma once

#include "util.hh"
#include "uvgrtp_export.hh"
#include "uvgrtp_defs.hh"

#include <map>
#include <string>


namespace uvgrtp {

    class session;
    class socketfactory;

    /**
     * \brief Provides CNAME isolation and can be used to create uvgrtp::session objects
     */
    class UVGRTP_EXPORT context {
        public:
            /**
             * \ingroup CORE_API
             * \brief RTP context constructor
             *
             * \details Most of the time one RTP context per application is enough.
             * If CNAME namespace isolation is required, multiple context objects can be created.
             */
            context();

            /**
             * \ingroup CORE_API
             * \brief RTP context destructor
             *
             * \details This does not destroy active sessions. They must be destroyed manually
             * by calling uvgrtp::context::destroy_session()
             */
            ~context();

            /**
             * \ingroup CORE_API
             * \brief Create a new RTP session
             *
             * \param address IP address of remote or local participant
             *
             * \return RTP session object
             *
             * \retval uvgrtp::session      On success
             * \retval nullptr               If "address" is empty or memory allocation failed
             */
            session* create_session(const char* address);

            /**
             * \ingroup CORE_API
             * \brief Create a new RTP session between two IP addresses
             *
             * \param addresses local and remote IP address for session
             *
             * \return RTP session object
             *
             * \retval uvgrtp::session     On success
             * \retval nullptr             If memory allocation failed
             */
            session* create_session(const char* local_address, const char* remote_address);

            /**
             * \ingroup CORE_API
             * \brief Destroy RTP session and all of its media streams
             *
             * \param session Pointer to the session object that should be destroyed
             *
             * \return RTP error code
             *
             * \retval RTP_OK                On success
             * \retval RTP_INVALID_VALUE     If session is nullptr
             */
            rtp_error_t destroy_session(uvgrtp::session* session);

            /**
             * \ingroup CORE_API
             * \brief Has Crypto++ been included in uvgRTP library
             *
             * \retval true      Crypto++ has been included, using SRTP is possible
             * \retval false     Crypto++ has not been included, using SRTP is not possible
             */
            bool crypto_enabled() const;

#if UVGRTP_EXTENDED_API

            /**
             * \ingroup EXTENDED_API
             * \brief Create a new RTP session between two IP addresses
             *
             * \param addresses Local and remote IP address for session as a pair
             *
             * \return RTP session object
             *
             * \retval uvgrtp::session     On success
             * \retval nullptr             If memory allocation failed
             */
            uvgrtp::session* create_session(std::pair<std::string, std::string> addresses);

            /**
             * \ingroup EXTENDED_API
             * \brief Create a new RTP session
             *
             * \param address IP address of remote or local participant
             *
             * \return RTP session object
             *
             * \retval uvgrtp::session      On success
             * \retval nullptr               If "address" is empty or memory allocation failed
             */
            uvgrtp::session *create_session(std::string address);

            /// \cond DO_NOT_DOCUMENT
            // Obsolete method, replaced by create_session(std::pair<std::string, std::string> addresses);
            uvgrtp::session *create_session(std::string remote_addr, std::string local_addr);
            /// \endcond
#endif


        private:

            class context_impl;
            context_impl* pimpl_;
        };
}

namespace uvg_rtp = uvgrtp;
