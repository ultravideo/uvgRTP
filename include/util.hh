#pragma once

#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#else
#include <sys/time.h>
#endif

#include <algorithm>
#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <string>

#if defined(_MSC_VER)
typedef SSIZE_T ssize_t;
#endif


#if defined(__MINGW32__) || defined(__MINGW64__) || defined(__linux__)
#define PACKED_STRUCT(name) \
    struct __attribute__((packed)) name
#else
//#warning "structures are not packed!"
#define PACKED_STRUCT(name) struct name
#endif

#ifdef _WIN32
    typedef SOCKET socket_t;
#else
    typedef int socket_t;
#endif

const int MAX_PACKET      = 65536;
const int MAX_PAYLOAD     = 1443;

typedef enum RTP_ERROR {
    RTP_INTERRUPTED     =  2,
    RTP_NOT_READY       =  1,
    RTP_OK              =  0,
    RTP_GENERIC_ERROR   = -1,
    RTP_SOCKET_ERROR    = -2,
    RTP_BIND_ERROR      = -3,
    RTP_INVALID_VALUE   = -4,
    RTP_SEND_ERROR      = -5,
    RTP_MEMORY_ERROR    = -6,
    RTP_SSRC_COLLISION  = -7,
    RTP_INITIALIZED     = -8,   /* object already initialized */
    RTP_NOT_INITIALIZED = -9,   /* object has not been initialized */
    RTP_NOT_SUPPORTED   = -10,  /* method/version/extension not supported */
    RTP_RECV_ERROR      = -11,  /* recv(2) or one of its derivatives failed */
    RTP_TIMEOUT         = -12,  /* operation timed out */
    RTP_NOT_FOUND       = -13,  /* object not found */
} rtp_error_t;

typedef enum RTP_FORMAT {
    RTP_FORMAT_GENERIC = 0,
    RTP_FORMAT_HEVC    = 96,
    RTP_FORMAT_OPUS    = 97,
} rtp_format_t;

typedef enum RTP_FLAGS {
    RTP_NO_FLAGS = 0 << 0,

    /* TODO  */
    RTP_SLICE    = 1 << 0,

    /* TODO */
    RTP_MORE     = 1 << 1,

    /* Make a copy of "data" given to push_frame()
     *
     * This is an easy way out of the memory ownership/deallocation problem
     * for the application but can significantly damage the performance
     *
     * NOTE: Copying is necessary only when the following conditions are met:
     *   - SCD is enabled
     *   - Media format is such that it uses SCD (HEVC is the only for now)
     *   - Application hasn't provided a deallocation callback
     *   - Application doens't use unique_ptrs
     *
     * If all of those conditions are met, then it's advised to pass RTP_COPY.
     * Otherwise there might be a lot of leaked memory */
    RTP_COPY = 1 << 2,
} rtp_flags_t;

/* These flags are given when uvgRTP context is created */
enum RTP_CTX_ENABLE_FLAGS {
    RTP_CTX_NO_FLAGS              = 0 << 0,

    /* Use optimistic receiver (HEVC only) */
    RCE_OPTIMISTIC_RECEIVER       = 1 << 0,

    /* Enable system call dispatcher (HEVC only) */
    RCE_SYSTEM_CALL_DISPATCHER    = 1 << 2,

    /* Use SRTP for this connection */
    RCE_SRTP                      = 1 << 3,

    /* Use ZRTP for key management
     *
     * If this flag is provided, before the session starts,
     * ZRTP will negotiate keys with the remote participants
     * and these keys are used as salting/keying material for the session.
     *
     * This flag must be coupled with RCE_SRTP and is mutually exclusive
     * with RCE_SRTP_KMNGMNT_USER. */
    RCE_SRTP_KMNGMNT_ZRTP         = 1 << 4,

    /* Use user-defined way to manage keys
     *
     * If this flag is provided, before the media transportation starts,
     * user must provide a master key and salt form which SRTP session
     * keys are derived
     *
     * This flag must be coupled with RCE_SRTP and is mutually exclusive
     * with RCE_SRTP_KMNGMNT_ZRTP */
    RCE_SRTP_KMNGMNT_USER         = 1 << 5,

    /* When uvgRTP is receiving HEVC stream, as an attempt to improve
     * QoS, it will set frame delay for intra frames to be the same
     * as intra period.
     *
     * What this means is that if the regular timer expires for frame
     * (100 ms) and the frame type is intra, uvgRTP will not drop the
     * frame but will continue receiving packets in hopes that all the
     * packets of the intra frame will be received and the frame can be
     * returned to user. During this period, when the intra frame is deemed
     * to be late and incomplete, uvgRTP will drop all inter frames until
     * a) all the packets of late intra frame are received or
     * b) a new intra frame is received
     *
     * This behaviour should reduce the number of gray screens during
     * HEVC decoding but might cause the video stream to freeze for a while
     * which is subjectively lesser of two evils
     *
     * This behavior can be disabled with RCE_HEVC_NO_INTRA_DELAY
     * If this flag is given, uvgRTP treats all frame types
     * equally and drops all frames that are late */
    RCE_HEVC_NO_INTRA_DELAY       = 1 << 5,

    /* Fragment generic frames into RTP packets of 1500 bytes.
     *
     * If RCE_FRAGMENT_GENERIC is given to create_stream(), uvgRTP will split frames
     * of type RTP_FORMAT_GENERIC into packets of 1500 bytes automatically and reconstruct
     * the full frame from the fragments in the receiver
     *
     * This behavior is not from any specification and only supported by uvgRTP so it
     * will break interoperability between libraries if enabled.
     *
     * RCE_FRAGMENT_GENERIC can be used, for example, when you're using uvgRTP for
     * both sender and receiver and the media stream you wish to stream is not supported
     * by uvgRTP but requires packetization because MEDIA_FRAME_SIZE > MTU */
    RCE_FRAGMENT_GENERIC          = 1 << 6,

    /* If SRTP is enabled and RCE_INPLACE_ENCRYPTION flag is *not* given,
     * uvgRTP will make a copy of the frame given to push_frame().
     *
     * If the frame is writable and the application no longer needs the frame,
     * RCE_INPLACE_ENCRYPTION should be given to create_stream() to prevent
     * unnecessary copy operations.
     *
     * If RCE_INPLACE_ENCRYPTION is given to push_frame(), the input pointer must be writable! */
    RCE_INPLACE_ENCRYPTION        = 1 << 7,

    /* Disable System Call Clustering (SCC), System Call Dispatching is still usable */
    RCE_NO_SYSTEM_CALL_CLUSTERING = 1 << 8,

    /* Make the media stream unidirectional, i.e. sender/receiver only */
    RCE_UNIDIRECTIONAL            = 1 << 9,

    /* Media stream is only used for sending
     * Mutually exclusive with RCE_UNIDIR_RECEIVER */
    RCE_UNIDIR_SENDER             = 1 << 10,

    /* Media stream is only used for receiving
     * Mutually exclusive with RCE_UNIDIR_SENDER */
    RCE_UNIDIR_RECEIVER           = 1 << 11,

    /* Disable RTP payload encryption */
    RCE_SRTP_NULL_CIPHER          = 1 << 12,

    RCE_LAST                      = 1 << 13,
};

/* These options are given to configuration() */
enum RTP_CTX_CONFIGURATION_FLAGS {
    /* No configuration flags */
    RCC_NO_FLAGS         = 0,

    /* How large is the receiver/sender UDP buffer size
     *
     * Default value is 4 MB
     *
     * For video with high bitrate, it is advisable to set this
     * to a high number to prevent OS from dropping packets */
    RCC_UDP_RCV_BUF_SIZE = 1,
    RCC_UDP_SND_BUF_SIZE = 2,

    RCC_LAST
};

enum NOTIFY_REASON {

    /* Timer for the active frame has expired and it has been dropped */
    NR_FRAME_DROPPED = 0,
};

/* see src/util.hh for more information */
typedef struct rtp_ctx_conf {
    int flags;
    ssize_t ctx_values[RCC_LAST];
} rtp_ctx_conf_t;

extern thread_local rtp_error_t rtp_errno;

#define TIME_DIFF(s, e, u) ((ssize_t)std::chrono::duration_cast<std::chrono::u>(e - s).count())

static inline void hex_dump(uint8_t *buf, size_t len)
{
    if (!buf)
        return;

    for (size_t i = 0; i < len; i += 10) {
        fprintf(stderr, "\t");
        for (size_t k = i; k < i + 10; ++k) {
            fprintf(stderr, "0x%02x ", buf[k]);
        }
        fprintf(stderr, "\n");
    }
}

static inline void set_bytes(int *ptr, int nbytes)
{
    if (ptr)
        *ptr = nbytes;
}

static inline std::string generate_string(size_t length)
{
    auto randchar = []() -> char
    {
        const char charset[] =
        "0123456789"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz";
        const size_t max_index = (sizeof(charset) - 1);
        return charset[ rand() % max_index ];
    };

    std::string str(length, 0);
    std::generate_n(str.begin(), length, randchar);
    return str;
}
