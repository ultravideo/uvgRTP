/// \file util.hh
#pragma once

/// \cond DO_NOT_DOCUMENT
#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#else
#include <sys/time.h>
#endif

// ssize_t definition for all systems
#if defined(_MSC_VER)
typedef SSIZE_T ssize_t;
#else
#include <sys/types.h>
#endif

#include <stdint.h>

/// \endcond

/**
 * \enum RTP_ERROR
 *
 * \brief RTP error codes
 *
 * \details These error valus are returned from various uvgRTP functions. Functions that return a pointer set rtp_errno global value that should be checked if a function call failed
 */
typedef enum RTP_ERROR {
    /// \cond DO_NOT_DOCUMENT
    RTP_MULTIPLE_PKTS_READY = 6,
    RTP_PKT_READY           = 5,
    RTP_PKT_MODIFIED        = 4,
    RTP_PKT_NOT_HANDLED     = 3,
    RTP_INTERRUPTED         = 2,
    RTP_NOT_READY           = 1,
    /// \endcond

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
    // See RFC 3551 for more details

    // static audio profiles
    RTP_FORMAT_GENERIC    = 0,   ///< Same as PCMU
    RTP_FORMAT_PCMU       = 0,   ///< PCMU, ITU-T G.711
    // 1 is reserved in RFC 3551 
    // 2 is reserved in RFC 3551
    RTP_FORMAT_GSM        = 3,   ///< GSM (Group Speciale Mobile)
    RTP_FORMAT_G723       = 4,   ///< G723
    RTP_FORMAT_DVI4_32    = 5,   ///< DVI 32 kbit/s
    RTP_FORMAT_DVI4_64    = 6,   ///< DVI 64 kbit/s
    RTP_FORMAT_LPC        = 7,   ///< LPC
    RTP_FORMAT_PCMA       = 8,   ///< PCMA
    RTP_FORMAT_G722       = 9,   ///< G722
    RTP_FORMAT_L16_STEREO = 10,  ///< L16 Stereo
    RTP_FORMAT_L16_MONO   = 11,  ///< L16 Mono
    // 12 QCELP is unsupported in uvgRTP
    // 13 CN is unsupported in uvgRTP
    // 14 MPA is unsupported in uvgRTP
    RTP_FORMAT_G728       = 15,  ///< G728
    RTP_FORMAT_DVI4_441   = 16,  ///< DVI 44.1 kbit/s
    RTP_FORMAT_DVI4_882   = 17,  ///< DVI 88.2 kbit/s
    RTP_FORMAT_G729       = 18,  ///< G729, 8 kbit/s
    // 19 is reserved in RFC 3551
    // 20 - 23 are unassigned in RFC 3551

    /* static video profiles, unsupported in uvgRTP
    * 24 is unassigned
    * 25 is CelB, 
    * 26 is JPEG
    * 27 is unassigned
    * 28 is nv
    * 29 is unassigned
    * 30 is unassigned
    * 31 is H261
    * 32 is MPV
    * 33 is MP2T
    * 32 is H263
    */

    /* Rest of static numbers
    * 35 - 71 are unassigned
    * 72 - 76 are reserved
    * 77 - 95 are unassigned
    */
    
    /* Formats with dynamic payload numbers 96 - 127, including default values.
    * Use RCC_DYN_PAYLOAD_TYPE flag to change the number if desired. */

    RTP_FORMAT_G726_40   = 96,  ///< G726, 40 kbit/s
    RTP_FORMAT_G726_32   = 97,  ///< G726, 32 kbit/s
    RTP_FORMAT_G726_24   = 98,  ///< G726, 24 kbit/s
    RTP_FORMAT_G726_16   = 99,  ///< G726, 16 kbit/s
    RTP_FORMAT_G729D     = 100, ///< G729D, 6.4 kbit/s
    RTP_FORMAT_G729E     = 101, ///< G729E, 11.8 kbit/s
    RTP_FORMAT_GSM_EFR   = 102, ///< GSM enhanced full rate speech transcoding
    RTP_FORMAT_L8        = 103, ///< L8, linear audio data samples
    // RED is unsupported in uvgRTP
    RTP_FORMAT_VDVI      = 104, ///< VDVI, variable-rate DVI4
    RTP_FORMAT_OPUS      = 105, ///< Opus, see RFC 7587
    // H263-1998 is unsupported in uvgRTP
    RTP_FORMAT_H264      = 106, ///< H.264/AVC, see RFC 6184
    RTP_FORMAT_H265      = 107, ///< H.265/HEVC, see RFC 7798
    RTP_FORMAT_H266      = 108  ///< H.266/VVC
    
} rtp_format_t;

/**
 * \enum RTP_FLAGS
 *
 * \brief These flags are given to uvgrtp::media_stream::push_frame()
 * and they can be OR'ed together
 */
typedef enum RTP_FLAGS {
    RTP_NO_FLAGS      = 0, ///<  Use this if you have no RTP flags

    /// \cond DO_NOT_DOCUMENT
    /** Obsolete flags*/
    RTP_OBSOLETE      = 1,
    RTP_SLICE         = 1, // used to do what RTP_NO_H26X_SCL does, may do something different in the future
    /// \endcond

    /** Make a copy of the frame and perform operation on the copy. Cannot be used with unique_ptr. */
    RTP_COPY          = 1 << 1,

    /** By default, uvgRTP searches for start code prefixes (0x000001 or 0x00000001)
     * from the frame to divide NAL units and remove the prefix. If you instead
     * want to provide the NAL units without the start code prefix yourself,
     * you may use this flag to disable Start Code Lookup (SCL) and the frames
     * will be treated as send-ready NAL units. */
    RTP_NO_H26X_SCL   = 1 << 2

} rtp_flags_t;

/**
 * \enum RTP_CTX_ENABLE_FLAGS
 *
 * \brief RTP context enable flags
 *
 * \details These flags are passed to uvgrtp::session::create_stream and can be OR'ed together
 */
enum RTP_CTX_ENABLE_FLAGS {
    RCE_NO_FLAGS                  = 0, ///<  Use this if you have no RCE flags

    /// \cond DO_NOT_DOCUMENT
    // Obsolete flags, they do nothing because the feature has been removed or they are enabled by default
    RCE_OBSOLETE                        = 1, ///< for checking if user inputs obsolete flags
    RCE_SYSTEM_CALL_DISPATCHER          = 1, ///< obsolete flag, removed feature
    RCE_NO_H26X_INTRA_DELAY             = 1, ///< obsolete flag,  removed feature
    RCE_NO_H26X_SCL                     = 1, ///< obsolete flag, this flag was moved to be an RTP flag
    RCE_H26X_NO_DEPENDENCY_ENFORCEMENT  = 1, ///< obsolete flag, the feature is disabled by default
    RCE_H26X_PREPEND_SC                 = 1, ///< obsolete flag, the feature is enabled by default
    RCE_NO_SYSTEM_CALL_CLUSTERING       = 1, ///< obsolete flag, disabled by default
    RCE_SRTP_INPLACE_ENCRYPTION         = 1, ///< obsolete flag, the feature is enabled by default

    // renamed flags
    RCE_H26X_DO_NOT_PREPEND_SC = 1 << 6,  ///< renamed flag, use RCE_NO_H26X_PREPEND_SC instead
    RCE_FRAMERATE              = 1 << 18, ///< renamed flag, use RCE_FRAME_RATE instead
    RCE_FRAGMENT_PACING        = 1 << 19, ///< renamed flag, use RCE_PACE_FRAGMENT_SENDING instead
    RCE_ZRTP_MULTISTREAM_NO_DH = 1 << 17, ///< renamed flag, use RCE_ZRTP_MULTISTREAM_MODE instead
    /// \endcond

    // These can be used to specify what the address does for one address create session
    RCE_SEND_ONLY                   = 1 << 1, ///<  address/port interpreted as remote, no binding to local socket
    RCE_RECEIVE_ONLY                = 1 << 2, ///<  address/port interpreted as local, sending not possible

    /** Use SRTP for this connection */
    RCE_SRTP                        = 1 << 3,

    /** Use ZRTP for key management
     *
     * If this flag is provided, before the session starts,
     * ZRTP will negotiate keys with the remote participants
     * and these keys are used as salting/keying material for the session.
     *
     * This flag must be coupled with RCE_SRTP and is mutually exclusive
     * with RCE_SRTP_KMNGMNT_USER. */
    RCE_SRTP_KMNGMNT_ZRTP           = 1 << 4,

    /** Use user-defined way to manage keys
     *
     * If this flag is provided, before the media transportation starts,
     * user must provide a master key and salt form which SRTP session
     * keys are derived
     *
     * This flag must be coupled with RCE_SRTP and is mutually exclusive
     * with RCE_SRTP_KMNGMNT_ZRTP */
    RCE_SRTP_KMNGMNT_USER           = 1 << 5,


    /** By default, uvgRTP restores the stream by prepending 3 or 4 byte start code to each received
     * H26x frame, so there is no difference with sender input. You can remove start code prefix with
     * this flag */
    RCE_NO_H26X_PREPEND_SC          = 1 << 6,

    /** Use this flag to discard inter frames that don't have their previous dependencies
        arrived. Does not work if the dependencies are not in monotonic order. */
    RCE_H26X_DEPENDENCY_ENFORCEMENT = 1 << 7,

    /** Fragment frames into RTP packets of MTU size (1492 bytes).
     *
     * Some RTP profiles define fragmentation by setting the marker bit indicating the 
     * last fragment of the frame. You can enable this functionality using this flag at 
     * both sender and receiver. 
     */
    RCE_FRAGMENT_GENERIC            = 1 << 8,

    /** Enable System Call Clustering (SCC). Sender side flag. 
    
    The benefit of SCC is reduced CPU usage at the sender, but its cost is increased chance of 
    losing frames at the receiving end due to too many packets arriving at once.*/
    RCE_SYSTEM_CALL_CLUSTERING      = 1 << 9,

    /** Disable RTP payload encryption */
    RCE_SRTP_NULL_CIPHER            = 1 << 10,

    /** Enable RTP packet authentication
     *
     * This flag forces the security layer to add authentication tag
     * to each outgoing RTP packet for all streams that have SRTP enabled.
     *
     * NOTE: this flag must be coupled with at least RCE_SRTP */
    RCE_SRTP_AUTHENTICATE_RTP       = 1 << 11,

    /** Enable packet replay protection */
    RCE_SRTP_REPLAY_PROTECTION      = 1 << 12,

    /** Enable RTCP for the media stream.
     * If SRTP is enabled, SRTCP is used instead */
    RCE_RTCP                        = 1 << 13,

    /** If the Mediastream object is used as a unidirectional stream
     * but holepunching has been enabled, this flag can be used to make
     * uvgRTP periodically send a short UDP datagram to keep the hole
     * in the firewall open */
    RCE_HOLEPUNCH_KEEPALIVE         = 1 << 14,

    /** Use 192-bit keys with SRTP, only user key management is supported */
    RCE_SRTP_KEYSIZE_192            = 1 << 15,

    /** Use 256-bit keys with SRTP, only user key management is supported */
    RCE_SRTP_KEYSIZE_256            = 1 << 16,

    /** Select which ZRTP stream performs the Diffie-Hellman exchange (default) */
    RCE_ZRTP_DIFFIE_HELLMAN_MODE    = 1 << 17,

    /** Select which ZRTP stream does not perform Diffie-Hellman exchange */
    RCE_ZRTP_MULTISTREAM_MODE       = 1 << 18,

    /** Force uvgRTP to send packets at certain framerate (default 30 fps) */
    RCE_FRAME_RATE                  = 1 << 19,

    /** Paces the sending of frame fragments within frame interval (default 1/30 s) */
    RCE_PACE_FRAGMENT_SENDING       = 1 << 20,

    RCE_RTCP_MUX                    = 1 << 21,
    
    /// \cond DO_NOT_DOCUMENT
    RCE_LAST                        = 1 << 22
   /// \endcond
}; // maximum is 1 << 30 for int


/**
 * \enum RTP_CTX_CONFIGURATION_FLAGS
 *
 * \brief RTP context configuration flags
 *
 * \details These flags are given to uvgrtp::media_stream::configure_ctx
 */
enum RTP_CTX_CONFIGURATION_FLAGS {
    /// \cond DO_NOT_DOCUMENT
    RCC_NO_FLAGS         = 0, // This flag has no purpose
    RCC_FPS_ENUMERATOR = 8, ///< renamed flag, use RCC_FPS_NUMERATOR instead
    /// \endcond

    /** How large is the receiver UDP buffer size
     *
     * Default value is 4 MB
     *
     * For video with high bitrate (100+ fps 4K), it is advisable to set this
     * to a high number to prevent OS from dropping packets */
    RCC_UDP_RCV_BUF_SIZE = 1,

    /** How large is the sender UDP buffer size
     *
     * Default value is 4 MB
     *
     * For video with high bitrate (100+ fps 4K), it is advisable to set this
     * to a high number to prevent OS from dropping packets */
    RCC_UDP_SND_BUF_SIZE = 2,

    /** How large is the uvgRTP receiver ring buffer
     *
     * Default value is 4 MB
     *
     * For video with high bitrate (100+ fps 4K), it is advisable to set this
     * to a high number to prevent uvgRTP from overwriting previous packets */
    RCC_RING_BUFFER_SIZE = 3,

    /** How many milliseconds is each frame waited for until it is considered lost.
     *
     * Default is 500 milliseconds
     *
     * This is valid only for fragmented frames,
     * i.e. RTP_FORMAT_H26X and RTP_FORMAT_GENERIC with RCE_FRAGMENT_GENERIC (TODO) */
    RCC_PKT_MAX_DELAY    = 4,

    /** Change uvgRTP's default payload number in RTP header */
    RCC_DYN_PAYLOAD_TYPE = 5,

    /** Change uvgRTP's clock rate in RTP header and RTP timestamp calculations */
    RCC_CLOCK_RATE       = 6,

    /** Set a maximum value for the Ethernet frame size assumed by uvgRTP.
     *
     * Default is 1492, from this IP and UDP, and RTP headers
     * are removed, giving a payload size of 1452 bytes.
     *
     * If application wishes to use small UDP datagram,
     * it can set MTU size to, for example, 500 bytes or if it wishes
     * to use jumbo frames, it can set the MTU size to 9000 bytes */
    RCC_MTU_SIZE         = 7,

    /** Set the numerator of frame rate used by uvgRTP.
    * 
    * Default is 30.
    * 
    * Setting the fps for uvgRTP serves two possible functions: 

    * 1) if RCE_FRAME_RATE has been set, the fps is enforced and 
    * uvgRTP tries to send frames at this exact frame rate, 
    
    2) if RCE_PACE_FRAGMENT_SENDING has been set, the fragments are set at a constant pace 
    * spaced out evenly within frame interval */
    RCC_FPS_NUMERATOR  = 8,

    /** Set the denominator of frame rate used by uvgRTP.
     *
     * Default is 1 
     *
     * See RCC_FPS_NUMERATOR for more info.
     */
    RCC_FPS_DENOMINATOR  = 9,

    /** Set the local SSRC of the stream manually
    *
    * By default local SSRC is generated randomly
    */
    RCC_SSRC = 10,

    /** Set the remote SSRC of the stream manually
    *
    * By default remote SSRC is generated randomly
    */
    RCC_REMOTE_SSRC = 11,

    /** Set bandwidth for the session
    * 
    * uvgRTP chooses this automatically depending on the format of the data being transferred.
    * Use this flag to manually set the session bandwidth in kbps. 
    * RTCP reporting interval depends on this session bandwidth. The interval is calculated with the
    * following formula:
    * 
    * RTCP interval = 1000 * 360 / session_bandwidth_kbps
    * 
    * Larger bandwidth values result in shorter RTCP intervals, and vice versa.
    * See RFC 3550 Appendix A.7 for further information on RTCP interval
    */
    RCC_SESSION_BANDWIDTH = 12,

    /** Set the timeout value for socket polling
    * 
    * Default value is 100 ms. If you are experiencing packet loss when receiving, you can try
    * lowering this value down to 0. This will, however cause increased CPU usage in the receiver, so
    * use with caution.
    */
    RCC_POLL_TIMEOUT       = 13,

    /// \cond DO_NOT_DOCUMENT
    RCC_LAST
    /// \endcond
};

extern thread_local rtp_error_t rtp_errno;
