#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#include <wincrypt.h>
#else
#include <sys/random.h>
#endif

#include <cstdlib>
#include <ctime>

#include "debug.hh"
#include "random.hh"

rtp_error_t kvz_rtp::random::init()
{
#ifdef _WIN32
    srand(GetTickCount());
#else
    srand(time(NULL));
#endif
    return RTP_OK;
}

int kvz_rtp::random::generate(void *buf, size_t n)
{
#ifdef __linux__
    return getrandom(buf, n, 0);
#else
    HCRYPTPROV hCryptProv;
    if (CryptAcquireContext(&hCryptProv, NULL, NULL, PROV_RSA_FULL, 0) == TRUE) {
        bool res = CryptGenRandom(hCryptProv, n, (BYTE *)buf);

        CryptReleaseContext(hCryptProv, 0);

        return res ? 0 : -1;
    }
    return -1;
#endif
}

uint32_t kvz_rtp::random::generate_32()
{
    uint32_t value;

    if (kvz_rtp::random::generate(&value, sizeof(uint32_t)) < 0)
        return rand();

    return value;
}

uint64_t kvz_rtp::random::generate_64()
{
    uint64_t value;

    if (kvz_rtp::random::generate(&value, sizeof(uint64_t)) < 0)
        return rand();

    return value;
}
