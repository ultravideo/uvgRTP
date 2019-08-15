#ifdef _WIN32
#else
#include <sys/random.h>
#endif

#include <cstdlib>
#include <ctime>

#include "debug.hh"
#include "random.hh"

#ifdef _WIN32
static HCRYPTPROV hCryptProv;
#endif

rtp_error_t kvz_rtp::random::init()
{
#ifdef _WIN32
    srand(GetTickCount());
#else
    srand(time(NULL));
#endif
}

int kvz_rtp::random::generate(void *buf, size_t n)
{
#ifdef __linux__
    return getrandom(buf, n, 0);
#else
    return CryptGenRandom(hCryptProv, n, buf);
#endif
}

uint32_t kvz_rtp::random::generate_32()
{
    uint32_t value;

    if (kvz_rtp::random::generate(&value, sizeof(uint32_t)) < 0) {
        LOG_WARN("Using fallback random number generator");
        return rand();
    }

    return value;
}

uint64_t kvz_rtp::random::generate_64()
{
    uint64_t value;

    if (kvz_rtp::random::generate(&value, sizeof(uint64_t)) < 0) {
        LOG_WARN("Using fallback random number generator");
        return rand();
    }

    return value;
}
