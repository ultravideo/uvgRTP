#include "random.hh"

#include "uvgrtp/debug.hh"

#if defined(HAVE_GETRANDOM)
#include <sys/random.h>
#elif defined(_WIN32)
#include <winsock2.h>
#include <windows.h>
#include <wincrypt.h>
#elif defined(__APPLE__)
#include <Security/SecRandom.h>
#endif

#ifndef _WIN32
#include <unistd.h>
#include <sys/syscall.h>
#endif


#include <cstdlib>
#include <ctime>

rtp_error_t uvgrtp::random::init()
{
#ifdef _WIN32
    srand(GetTickCount());
#else
    srand(time(NULL));
#endif
    return RTP_OK;
}

int uvgrtp::random::generate(void *buf, size_t n)
{
#if defined(HAVE_GETRANDOM)

    return getrandom(buf, n, 0);

#elif defined(SYS_getrandom)

    // Replace with the syscall
    int read = syscall(SYS_getrandom, buf, n, 0);

    // On error, return the same value as getrandom()
    if (read == -EINTR || read == -ERESTART) {
        errno = EINTR;
        read = -1;
    }

    if (read < -1) {
        errno = -read;
        read = -1;
    }

    return read;

#elif defined(_WIN32)

    if (n > UINT32_MAX)
    {
        LOG_WARN("Tried to generate too large random number");
        n = UINT32_MAX;
    }

    HCRYPTPROV hCryptProv;
    if (CryptAcquireContext(&hCryptProv, NULL, NULL, PROV_RSA_FULL, 0) == TRUE) {
        bool res = CryptGenRandom(hCryptProv, (DWORD)n, (BYTE *)buf);

        CryptReleaseContext(hCryptProv, 0);

        return res ? 0 : -1;
    }
    return -1;

#elif defined(__APPLE__)

    int status = SecRandomCopyBytes(kSecRandomDefault, n, buf);
    if (status == errSecSuccess)
        return n;
    return -1;

#endif

    return -1;
}

uint32_t uvgrtp::random::generate_32()
{
    uint32_t value;

    if (uvgrtp::random::generate(&value, sizeof(uint32_t)) < 0)
        return rand();

    return value;
}

uint64_t uvgrtp::random::generate_64()
{
    uint64_t value;

    if (uvgrtp::random::generate(&value, sizeof(uint64_t)) < 0)
        return rand();

    return value;
}
