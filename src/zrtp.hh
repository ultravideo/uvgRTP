#pragma once

#ifdef _WIN32
#include <winsock2.h>
#include <mswsock.h>
#include <inaddr.h>
#else
#include <netinet/ip.h>
#include <arpa/inet.h>
#endif

#include <vector>

#include "util.hh"

namespace kvz_rtp {

    typedef struct capabilities {

        /* Supported ZRTP version */
        uint32_t version;

        /* ZID of this ZRTP instance */
        uint8_t *zid;

        /* Header of the supported algos etc. */
        uint32_t header;

        /* Supported hash algorithms (empty for us) */
        std::vector<uint32_t> hash_algos;

        /* Supported cipher algorithms (empty for us) */
        std::vector<uint32_t> cipher_algos;

        /* Supported authentication tag types (empty for us) */
        std::vector<uint32_t> auth_tags;

        /* Supported Key Agreement types (empty for us) */
        std::vector<uint32_t> key_agreements;

        /* Supported SAS types (empty for us) */
        std::vector<uint32_t> sas_types;
    } zrtp_capab_t;

    typedef struct dh_commit {
    } dh_commit_t;

    class zrtp {
        public:
            zrtp();
            ~zrtp();

            /* TODO:  */
            rtp_error_t init(uint32_t ssrc, socket_t& socket, sockaddr_in& addr);

        private:

            /* Set timeout for a socket, needed by backoff timers of ZRTP
             *
             * "timeout" tells the timeout in milliseconds
             *
             * Return RTP_OK on success
             * Return RTP_GENERIC_ERROR if timeout could not be set */
            rtp_error_t set_timeout(size_t timeout);

            /* Get our own capabilities, see struct capabilities above for more details */
            zrtp_capab_t get_capabilities();

            /* Generate zid for this ZRTP instance. ZID is a unique, 96-bit long ID */
            uint8_t *generate_zid();

            /* Being the ZRTP session by sending a Hello message to remote,
             * and responding to remote's Hello message using HelloAck message 
             *
             * If session begins successfully, remote zrtp_capab_t are put into
             * "remote_capab" for later use
             *
             * Return RTP_OK on success 
             * Return RTP_NOT_SUPPORTED if remote did not answer to our Hello messages */
            rtp_error_t begin_session();

            uint32_t ssrc_;
            socket_t socket_;
            sockaddr_in addr_;

            /* Our own and remote capability structs */
            zrtp_capab_t capab_;
            zrtp_capab_t rcapab_;

            uint8_t *zid_;
    };
};
