
#include "uvgrtp/util.hh"

#include <cstdint>

namespace uvgrtp {
    namespace random {

        /* Initialize the PSRNG of standard lib and for windows
         * acquire the cryptographic context
         *
         * Return RTP_OK on success
         * Return RTP_GENERIC_ERROR if acquiring the the crypt context fails */
        rtp_error_t init();

        int generate(void *buf, size_t n);
        uint32_t generate_32();
        uint64_t generate_64();
    }
}

namespace uvg_rtp = uvgrtp;
