#pragma once

#include "util.hh"

#include "uvgrtp/export.hh"
#include "uvgrtp/definitions.hh"

#include <unordered_map>
#include <memory>
#include <string>
#include <atomic>
#include <cstdint>

#ifndef _WIN32
#include <sys/socket.h>
#include <netinet/in.h>
#else
#include <ws2ipdef.h>
#endif
namespace uvgrtp {

    // forward declarations
    /// \cond DO_NOT_DOCUMENT
    class rtcp;
    class socketfactory;

    namespace frame {
        struct rtp_frame;
    }

    namespace formats {
        class media;
    }

    class media_stream_internal;
    /// \endcond

    /**
     * \brief The media_stream is an entity which represents one RTP stream.
     *
     * \details media_stream is defined by the ports which are used for sending and/or receiving media. 
     * It is possible for media_stream to be bi- or unidirectional. The unidirectionality 
     * is achieved by specifying RCE_SEND_ONLY or RCE_RECEIVE_ONLY flag when creating media_stream. 
     * 
     * If RCE_RTCP was given when creating media_stream, you can get the uvgrtp::rtcp object with get_rtcp()-function.
     *
     * media_stream corresponds to one RTP session in <a href="https://www.rfc-editor.org/rfc/rfc3550">RFC 3550</a>.
     */
    class UVGRTP_EXPORT media_stream {
        /// \cond DO_NOT_DOCUMENT
        friend class session;
        /// \endcond
        public:
            /**
             * \ingroup CORE_API
             * \brief Start the ZRTP negotiation manually.
             *
             * \details There are two ways to use ZRTP:
             * 1. Use flags RCE_SRTP + RCE_SRTP_KMNGMNT_ZRTP + (RCE_ZRTP_DIFFIE_HELLMAN_MODE/RCE_ZRTP_MULTISTREAM_MODE)
             *    to automatically start ZRTP negotiation when creating a media stream.
             * 2. Use flags RCE_SRTP + (RCE_ZRTP_DIFFIE_HELLMAN_MODE/RCE_ZRTP_MULTISTREAM_MODE) and after creating
             *    the media stream, call start_zrtp() to manually start the ZRTP negotiation.
             *
             * \return RTP error code
             *
             * \retval RTP_OK On success
             * \retval RTP_TIMEOUT If ZRTP timed out
             * \retval RTP_GENERIC_ERROR On other errors
             */
            rtp_error_t start_zrtp();

            /**
             * \ingroup CORE_API
             * \brief Add keying information for user-managed SRTP session
             *
             * \details For user-managed SRTP session (flag RCE_SRTP_KMNGMNT_USER),
             * the media stream is not started until SRTP key has been added and all calls
             * to push_frame() will fail.
             *
             * \param key SRTP master key, default is 128-bit long
             * \param salt 112-bit long salt
             *
             * \return RTP error code
             *
             * \retval RTP_OK On success
             * \retval RTP_INVALID_VALUE If key or salt is invalid
             * \retval RTP_NOT_SUPPORTED If user-managed SRTP was not specified in create_stream()
             */
            rtp_error_t add_srtp_ctx(uint8_t* key, uint8_t* salt);

            /**
             * \ingroup CORE_API
             * \brief Send data to remote participant with a custom timestamp
             *
             * \details If so specified either by the selected media format and/or given
             * ::RTP_CTX_ENABLE_FLAGS, uvgRTP fragments the input data into RTP packets of 1492 bytes,
             * or to any other size defined by the application using ::RCC_MTU_SIZE.
             *
             * The frame is automatically reconstructed by the receiver if all fragments have been
             * received successfully.
             *
             * \param data Pointer to data that should be sent, uvgRTP does not take ownership of the memory
             * \param data_len Length of data
             * \param rtp_flags Optional flags, see ::RTP_FLAGS for more details
             *
             * \return RTP error code
             *
             * \retval RTP_OK On success
             * \retval RTP_INVALID_VALUE If one of the parameters is invalid
             * \retval RTP_MEMORY_ERROR If the data chunk is too large to be processed
             * \retval RTP_SEND_ERROR If uvgRTP failed to send the data to remote
             * \retval RTP_GENERIC_ERROR If an unspecified error occurred
             */
            rtp_error_t push_frame(uint8_t* data, size_t data_len, int rtp_flags);

            /**
             * \ingroup CORE_API
             * \brief Send data to remote participant with a custom timestamp
             *
             * \details If so specified either by the selected media format and/or given
             * ::RTP_CTX_ENABLE_FLAGS, uvgRTP fragments the input data into RTP packets of 1492 bytes,
             * or to any other size defined by the application using ::RCC_MTU_SIZE.
             *
             * The frame is automatically reconstructed by the receiver if all fragments have been
             * received successfully.
             *
             * If the application wishes, it may override uvgRTP's own timestamp
             * calculations and provide timestamping information for the stream itself.
             * This requires that the application provides a sensible value for the `ts`
             * parameter. If RTCP has been enabled, uvgrtp::rtcp::set_ts_info() should have
             * been called.
             *
             * \param data Pointer to data that should be sent, uvgRTP does not take ownership of the memory
             * \param data_len Length of data
             * \param ts 32-bit timestamp value for the data
             * \param rtp_flags Optional flags, see ::RTP_FLAGS for more details
             *
             * \return RTP error code
             *
             * \retval RTP_OK On success
             * \retval RTP_INVALID_VALUE If one of the parameters is invalid
             * \retval RTP_MEMORY_ERROR If the data chunk is too large to be processed
             * \retval RTP_SEND_ERROR If uvgRTP failed to send the data to remote
             * \retval RTP_GENERIC_ERROR If an unspecified error occurred
             */
            rtp_error_t push_frame(uint8_t* data, size_t data_len, uint32_t ts, int rtp_flags);

            /**
             * \ingroup CORE_API
             * \brief Send data to remote participant with custom RTP and NTP timestamps
             *
             * \details If so specified either by the selected media format and/or given
             * ::RTP_CTX_ENABLE_FLAGS, uvgRTP fragments the input data into RTP packets of 1492 bytes,
             * or to any other size defined by the application using ::RCC_MTU_SIZE.
             *
             * The frame is automatically reconstructed by the receiver if all fragments have been
             * received successfully.
             *
             * If the application wishes, it may override uvgRTP's own timestamp
             * calculations and provide timestamping information for the stream itself.
             * This requires that the application provides a sensible value for the `ts`
             * parameter. If RTCP has been enabled, uvgrtp::rtcp::set_ts_info() should have
             * been called.
             *
             * \param data Pointer to data that should be sent, uvgRTP does not take ownership of the memory
             * \param data_len Length of data
             * \param ts 32-bit RTP timestamp for the packet
             * \param ntp_ts 64-bit NTP timestamp value of when the packet's data was sampled. NTP timestamp is a
             *  64-bit unsigned fixed-point number with the integer part (seconds) in the first 32 bits and the
             *  fractional part (fractional seconds) in the last 32 bits. Used for synchronizing multiple streams.
             * \param rtp_flags Optional flags, see ::RTP_FLAGS for more details
             *
             * \return RTP error code
             *
             * \retval RTP_OK On success
             * \retval RTP_INVALID_VALUE If one of the parameters is invalid
             * \retval RTP_MEMORY_ERROR If the data chunk is too large to be processed
             * \retval RTP_SEND_ERROR If uvgRTP failed to send the data to remote
             * \retval RTP_GENERIC_ERROR If an unspecified error occurred
             */
            rtp_error_t push_frame(uint8_t* data, size_t data_len, uint32_t ts, uint64_t ntp_ts, int rtp_flags);

            /**
             * \ingroup CORE_API
             * \brief Poll a frame indefinitely from the media stream object
             *
             * \return RTP frame
             *
             * \retval uvgrtp::frame::rtp_frame* On success
             * \retval nullptr If an unrecoverable error occurred
             */
            uvgrtp::frame::rtp_frame* pull_frame();

            /**
             * \ingroup CORE_API
             * \brief Poll a frame for a specified time from the media stream object
             *
             * \param timeout_ms How long to wait for a frame, in milliseconds
             *
             * \return RTP frame
             *
             * \retval uvgrtp::frame::rtp_frame* On success
             * \retval nullptr If a frame was not received within the specified time limit or in case of an error
             */
            uvgrtp::frame::rtp_frame* pull_frame(size_t timeout_ms);

            /**
             * \ingroup CORE_API
             * \brief Asynchronous way of getting frames
             *
             * \details The receive hook is an alternative to polling frames using uvgrtp::media_stream::pull_frame().
             * Instead of the application asking from uvgRTP if there are any new frames available, uvgRTP will notify
             * the application when a frame has been received.
             *
             * The hook should not be used for media processing as it will block the receiver from
             * reading more frames. Instead, it should only be used as an interface between uvgRTP and
             * the calling application where the frame hand-off happens.
             *
             * \param arg Optional argument that is passed to the hook when it is called, can be set to nullptr
             * \param hook Function pointer to the receive hook that uvgRTP should call
             *
             * \return RTP error code
             *
             * \retval RTP_OK On success
             * \retval RTP_INVALID_VALUE If hook is nullptr
             */
            rtp_error_t install_receive_hook(void* arg, void (*hook)(void*, uvgrtp::frame::rtp_frame*));

            /**
             * \ingroup CORE_API
             * \brief Configure the media stream, see ::RTP_CTX_CONFIGURATION_FLAGS for more details
             *
             * \return RTP error code
             *
             * \retval RTP_OK On success
             * \retval RTP_INVALID_VALUE If the provided value is not valid for a given configuration flag
             * \retval RTP_GENERIC_ERROR If setsockopt(2) failed
             */
            rtp_error_t configure_ctx(int rcc_flag, ssize_t value);

            /**
             * \ingroup CORE_API
             * \brief Get the values associated with configuration flags, see ::RTP_CTX_CONFIGURATION_FLAGS for more details
             *
             * \return Value of the configuration flag
             *
             * \retval int value on success
             * \retval -1 on error
             */
            int get_configuration_value(int rcc_flag);

            /**
             * \ingroup CORE_API
             * \brief Get pointer to the RTCP object of the media stream
             *
             * \details This object is used to control all RTCP-related functionality
             * and RTCP documentation can be found in \ref uvgrtp::rtcp
             *
             * \return Pointer to RTCP object
             *
             * \retval uvgrtp::rtcp* If RTCP has been enabled (RCE_RTCP has been given to uvgrtp::session::create_stream())
             * \retval nullptr If RTCP has not been enabled
             */
            uvgrtp::rtcp* get_rtcp();

            /**
             * \ingroup CORE_API
             * \brief Get SSRC identifier. You can use the SSRC value for example to find the report
             * block belonging to this media stream in RTCP sender/receiver report.
             *
             * \return SSRC value
             */
            uint32_t get_ssrc() const;



#if UVGRTP_EXTENDED_API

            /** \ingroup EXTENDED_API
             *  @{
             */

            /**
             * \brief Send data to remote participant with a custom timestamp
             *
             * \details If so specified either by the selected media format and/or given
             * ::RTP_CTX_ENABLE_FLAGS, uvgRTP fragments the input data into RTP packets of 1492 bytes,
             * or to any other size defined by the application using ::RCC_MTU_SIZE
             *
             * The frame is automatically reconstructed by the receiver if all fragments have been
             * received successfully.
             *
             * \param data Smart pointer to data the that should be sent
             * \param data_len Length of data
             * \param rtp_flags Optional flags, see ::RTP_FLAGS for more details
             *
             * \return RTP error code
             *
             * \retval  RTP_OK            On success
             * \retval  RTP_INVALID_VALUE If one of the parameters are invalid
             * \retval  RTP_MEMORY_ERROR  If the data chunk is too large to be processed
             * \retval  RTP_SEND_ERROR    If uvgRTP failed to send the data to remote
             * \retval  RTP_GENERIC_ERROR If an unspecified error occurred
             */
            rtp_error_t push_frame(std::unique_ptr<uint8_t[]> data, size_t data_len, int rtp_flags);

            /**
             * \brief Send data to remote participant with a custom timestamp
             *
             * \details If so specified either by the selected media format and/or given
             * ::RTP_CTX_ENABLE_FLAGS, uvgRTP fragments the input data into RTP packets of 1492 bytes,
             * or to any other size defined by the application using ::RCC_MTU_SIZE
             *
             * The frame is automatically reconstructed by the receiver if all fragments have been
             * received successfully.
             *
             * If application so wishes, it may override uvgRTP's own timestamp
             * calculations and provide timestamping information for the stream itself.
             * This requires that the application provides a sensible value for the ts
             * parameter. If RTCP has been enabled, uvgrtp::rtcp::set_ts_info() should have
             * been called.
             *
             * \param data Smart pointer to data the that should be sent
             * \param data_len Length of data
             * \param ts 32-bit timestamp value for the data
             * \param rtp_flags Optional flags, see ::RTP_FLAGS for more details
             *
             * \return RTP error code
             *
             * \retval  RTP_OK            On success
             * \retval  RTP_INVALID_VALUE If one of the parameters are invalid
             * \retval  RTP_MEMORY_ERROR  If the data chunk is too large to be processed
             * \retval  RTP_SEND_ERROR    If uvgRTP failed to send the data to remote
             * \retval  RTP_GENERIC_ERROR If an unspecified error occurred
             */
            rtp_error_t push_frame(std::unique_ptr<uint8_t[]> data, size_t data_len, uint32_t ts, int rtp_flags);


            /**
             * \brief Send data to remote participant with custom RTP and NTP timestamps
             *
             * \details If so specified either by the selected media format and/or given
             * ::RTP_CTX_ENABLE_FLAGS, uvgRTP fragments the input data into RTP packets of 1492 bytes,
             * or to any other size defined by the application using ::RCC_MTU_SIZE
             *
             * The frame is automatically reconstructed by the receiver if all fragments have been
             * received successfully.
             *
             * If application so wishes, it may override uvgRTP's own timestamp
             * calculations and provide timestamping information for the stream itself.
             * This requires that the application provides a sensible value for the ts
             * parameter. If RTCP has been enabled, uvgrtp::rtcp::set_ts_info() should have
             * been called.
             *
             * \param data Smart pointer to data the that should be sent
             * \param data_len Length of data
             * \param ts 32-bit RTP timestamp for the packet
             * \param ntp_ts 64-bit NTP timestamp value of when the packets data was sampled. NTP timestamp is a
             *  64-bit unsigned fixed-point number with the integer part (seconds) in the first 32 bits and the
             *  fractional part (fractional seconds) in the last 32 bits. Used for synchronizing multiple streams.
             * \param rtp_flags Optional flags, see ::RTP_FLAGS for more details
             *
             * \return RTP error code
             *
             * \retval  RTP_OK            On success
             * \retval  RTP_INVALID_VALUE If one of the parameters are invalid
             * \retval  RTP_MEMORY_ERROR  If the data chunk is too large to be processed
             * \retval  RTP_SEND_ERROR    If uvgRTP failed to send the data to remote
             * \retval  RTP_GENERIC_ERROR If an unspecified error occurred
             */
            rtp_error_t push_frame(std::unique_ptr<uint8_t[]> data, size_t data_len, uint32_t ts, uint64_t ntp_ts, int rtp_flags);

            /** @} */  // End of EXTENDED_API group
#endif

        private:

            media_stream(std::string cname, std::string remote_addr, std::string local_addr, uint16_t src_port, uint16_t dst_port,
                rtp_format_t fmt, std::shared_ptr<uvgrtp::socketfactory> sfp, int rce_flags, uint32_t ssrc = 0);
            ~media_stream();

           media_stream_internal* impl_;
    };
}

namespace uvg_rtp = uvgrtp;
