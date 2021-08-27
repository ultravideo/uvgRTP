/// \file util.hh
#pragma once

/// \cond DO_NOT_DOCUMENT
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
#include <cstring>
#include <string>

#if defined(_MSC_VER)
typedef SSIZE_T ssize_t;
#endif

/* https://stackoverflow.com/questions/1537964/visual-c-equivalent-of-gccs-attribute-packed  */
#if defined(__MINGW32__) || defined(__MINGW64__) || defined(__GNUC__) || defined(__linux__)
#define PACK(__Declaration__) __Declaration__ __attribute__((__packed__))
#else
#define PACK(__Declaration__) __pragma(pack(push, 1)) __Declaration__ __pragma(pack(pop))
#endif

#ifdef _WIN32
    typedef SOCKET socket_t;
#else
    typedef int socket_t;
#endif

const int MAX_PACKET       = 65536;
const int MAX_PAYLOAD      = 1446;
const int PKT_MAX_DELAY    = 100;

/* TODO: add ability for user to specify these? */
enum HEADER_SIZES {
    ETH_HDR_SIZE  = 14,
    IPV4_HDR_SIZE = 20,
    UDP_HDR_SIZE  =  8,
    RTP_HDR_SIZE  = 12
};
/// \endcond

/**
 * \enum RTP_ERROR
 *
 * \brief RTP error codes
 *
 * \details These error valus are returned from various uvgRTP functions. Functions that return a pointer set rtp_errno global value that should be checked if a function call failed
 */
typedef enum RTP_ERROR {
    RTP_MULTIPLE_PKTS_READY = 6,
    RTP_PKT_READY           = 5,
    RTP_PKT_MODIFIED        = 4,
    RTP_PKT_NOT_HANDLED     = 3,
    RTP_INTERRUPTED         = 2,
    RTP_NOT_READY           = 1,
    RTP_OK                  = 0,    ///< Success
    RTP_GENERIC_ERROR       = -1,   ///< Generic error condition
    RTP_SOCKET_ERROR        = -2,   ///< Failed to create socket
    RTP_BIND_ERROR          = -3,   ///< Failed to bind to interface
    RTP_INVALID_VALUE       = -4,   ///< Invalid value
    RTP_SEND_ERROR          = -5,   ///< System call send(2) or one of its derivatives failed
    RTP_MEMORY_ERROR        = -6,   ///< Memory allocation failed
    RTP_SSRC_COLLISION      = -7,   ///< SSRC collision detected
    RTP_INITIALIZED         = -8,   ///< Object already initialized
    RTP_NOT_INITIALIZED     = -9,   ///< Object has not been initialized
    RTP_NOT_SUPPORTED       = -10,  ///< Method/version/extension not supported
    RTP_RECV_ERROR          = -11,  ///< System call recv(2) or one of its derivatives failed
    RTP_TIMEOUT             = -12,  ///< Operation timed out
    RTP_NOT_FOUND           = -13,  ///< Object not found
    RTP_AUTH_TAG_MISMATCH   = -14,  ///< Authentication tag does not match the RTP packet contents
} rtp_error_t;

/**
 * \enum RTP_FORMAT
 *
 * \brief These flags are given to uvgrtp::session::create_stream()
 */
typedef enum RTP_FORMAT {
    RTP_FORMAT_GENERIC = 0,   ///< Generic format
    RTP_FORMAT_H264    = 95,  ///< H.264/AVC
    RTP_FORMAT_H265    = 96,  ///< H.265/HEVC
    RTP_FORMAT_H266    = 97,  ///< H.266/VVC
    RTP_FORMAT_OPUS    = 98,  ///< Opus
} rtp_format_t;

/**
 * \enum RTP_FLAGS
 *
 * \brief These flags are given to uvgrtp::media_stream::push_frame()
 * and they can be OR'ed together
 */
typedef enum RTP_FLAGS {
    /** No flags */
    RTP_NO_FLAGS = 0 << 0,

    /** Treat the incoming frame as an H26X slice unit and do not perform start code lookup on it */
    RTP_SLICE    = 1 << 0,

    /** Make a copy of the data given to push_frame() and operate on that */
    RTP_COPY     = 1 << 1

} rtp_flags_t;

/**
 * \enum RTP_CTX_ENABLE_FLAGS
 *
 * \brief RTP context enable flags
 *
 * \details These flags are passed to uvgrtp::session::create_stream and can be OR'ed together
 */
enum RTP_CTX_ENABLE_FLAGS {
    RCE_NO_FLAGS                  = 0 << 0,

    /* Enable system call dispatcher (HEVC only) */
    RCE_SYSTEM_CALL_DISPATCHER    = 1 << 2,

    /** Use SRTP for this connection */
    RCE_SRTP                      = 1 << 3,

    /** Use ZRTP for key management
     *
     * If this flag is provided, before the session starts,
     * ZRTP will negotiate keys with the remote participants
     * and these keys are used as salting/keying material for the session.
     *
     * This flag must be coupled with RCE_SRTP and is mutually exclusive
     * with RCE_SRTP_KMNGMNT_USER. */
    RCE_SRTP_KMNGMNT_ZRTP         = 1 << 4,

    /** Use user-defined way to manage keys
     *
     * If this flag is provided, before the media transportation starts,
     * user must provide a master key and salt form which SRTP session
     * keys are derived
     *
     * This flag must be coupled with RCE_SRTP and is mutually exclusive
     * with RCE_SRTP_KMNGMNT_ZRTP */
    RCE_SRTP_KMNGMNT_USER         = 1 << 5,

    /** When uvgRTP is receiving H26X stream, as an attempt to improve
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
     * video decoding but might cause the video stream to freeze for a while
     * which is subjectively lesser of two evils
     *
     * This behavior can be disabled with RCE_NO_H26X_INTRA_DELAY
     * If this flag is given, uvgRTP treats all frame types
     * equally and drops all frames that are late */
    RCE_NO_H26X_INTRA_DELAY       = 1 << 5,

    /** Fragment generic frames into RTP packets of 1500 bytes.
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

    /** If SRTP is enabled and RCE_INPLACE_ENCRYPTION flag is *not* given,
     * uvgRTP will make a copy of the frame given to push_frame().
     *
     * If the frame is writable and the application no longer needs the frame,
     * RCE_INPLACE_ENCRYPTION should be given to create_stream() to prevent
     * unnecessary copy operations.
     *
     * If RCE_INPLACE_ENCRYPTION is given to push_frame(), the input pointer must be writable! */
    RCE_SRTP_INPLACE_ENCRYPTION   = 1 << 7,

    /** Disable System Call Clustering (SCC) */
    RCE_NO_SYSTEM_CALL_CLUSTERING = 1 << 8,

    /** Disable RTP payload encryption */
    RCE_SRTP_NULL_CIPHER          = 1 << 9,

    /** Enable RTP packet authentication
     *
     * This flag forces the security layer to add authentication tag
     * to each outgoing RTP packet for all streams that have SRTP enabled.
     *
     * NOTE: this flag must be coupled with at least RCE_SRTP */
    RCE_SRTP_AUTHENTICATE_RTP     = 1 << 10,

    /** Enable packet replay protection */
    RCE_SRTP_REPLAY_PROTECTION    = 1 << 11,

    /** Enable RTCP for the media stream.
     * If SRTP is enabled, SRTCP is used instead */
    RCE_RTCP                      = 1 << 12,

    /** Prepend a 4-byte start code (0x00000001) to HEVC each frame */
    RCE_H26X_PREPEND_SC           = 1 << 13,

    /** If the Mediastream object is used as a unidirectional stream
     * but holepunching has been enabled, this flag can be used to make
     * uvgRTP periodically send a short UDP datagram to keep the hole
     * in the firewall open */
    RCE_HOLEPUNCH_KEEPALIVE       = 1 << 14,

    /** Use 192-bit keys with SRTP */
    RCE_SRTP_KEYSIZE_192          = 1 << 15,

    /** Use 256-bit keys with SRTP */
    RCE_SRTP_KEYSIZE_256          = 1 << 16,

    RCE_LAST                      = 1 << 17,
};

/**
 * \enum RTP_CTX_CONFIGURATION_FLAGS
 *
 * \brief RTP context configuration flags
 *
 * \details These flags are given to uvgrtp::media_stream::configure_ctx
 */
enum RTP_CTX_CONFIGURATION_FLAGS {
    RCC_NO_FLAGS         = 0,

    /** How large is the receiver UDP buffer size
     *
     * Default value is 4 MB
     *
     * For video with high bitrate, it is advisable to set this
     * to a high number to prevent OS from dropping packets */
    RCC_UDP_RCV_BUF_SIZE = 1,

    /** How large is the sender UDP buffer size
     *
     * Default value is 4 MB
     *
     * For video with high bitrate, it is advisable to set this
     * to a high number to prevent OS from dropping packets */
    RCC_UDP_SND_BUF_SIZE = 2,

    /** How many milliseconds is each frame waited until it's dropped
     *
     * Default is 100 milliseconds
     *
     * This is valid only for fragmented frames,
     * i.e. RTP_FORMAT_H26X and RTP_FORMAT_GENERIC with RCE_FRAGMENT_GENERIC (TODO) */
    RCC_PKT_MAX_DELAY    = 3,

    /** Overwrite uvgRTP's own payload type in RTP packets and specify your own
     * dynamic payload type for all packets of an RTP stream */
    RCC_DYN_PAYLOAD_TYPE = 4,

    /** Set a maximum value for the Ethernet frame size assumed by uvgRTP.
     *
     * Default is 1500, from this Ethernet, IPv4 and UDP, and RTP headers
     * are removed from this, giving a payload size of 1446 bytes
     *
     * If application wishes to use small UDP datagrams for some reason,
     * it can set MTU size to, for example, 500 bytes or if it wishes
     * to use jumbo frames, it can set the MTU size to 9000 bytes */
    RCC_MTU_SIZE         = 5,

    RCC_LAST
};

/// \cond DO_NOT_DOCUMENT
enum NOTIFY_REASON {

    /* Timer for the active frame has expired and it has been dropped */
    NR_FRAME_DROPPED = 0,
};

/* see src/util.hh for more information */
typedef struct rtp_ctx_conf {
    int flags = 0;
    ssize_t ctx_values[RCC_LAST];
} rtp_ctx_conf_t;

extern thread_local rtp_error_t rtp_errno;

#define TIME_DIFF(s, e, u) ((ssize_t)std::chrono::duration_cast<std::chrono::u>(e - s).count())

#define SET_NEXT_FIELD_32(a, p, v) do { *(uint32_t *)&(a)[p] = (v); p += 4; } while (0)
#define SET_FIELD_32(a, i, v)      do { *(uint32_t *)&(a)[i] = (v); } while (0)

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

static inline void *memdup(const void *src, size_t len)
{
    uint8_t *dst = new uint8_t[len];
    std::memcpy(dst, src, len);

    return dst;
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
/// \endcond
