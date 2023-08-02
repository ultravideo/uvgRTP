#pragma once

#include "util.hh"

#include <unordered_map>
#include <memory>
#include <string>
#include <atomic>

#ifndef _WIN32
#include <sys/socket.h>
#include <netinet/in.h>
#else
#include <ws2ipdef.h>
#endif
namespace uvgrtp {

    // forward declarations
    class rtp;
    class rtcp;

    class zrtp;
    class base_srtp;
    class srtp;
    class srtcp;

    class reception_flow;
    class holepuncher;
    class socket;
    class socketfactory;
    class rtcp_reader;

    namespace frame {
        struct rtp_frame;
    }

    namespace formats {
        class media;
    }

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
    class media_stream {
        public:
            /// \cond DO_NOT_DOCUMENT
            media_stream(std::string cname, std::string remote_addr, std::string local_addr, uint16_t src_port, uint16_t dst_port,
                rtp_format_t fmt, std::shared_ptr<uvgrtp::socketfactory> sfp, int rce_flags);
            ~media_stream();

            /* Initialize traditional RTP session.
             * Allocate Connection/Reader/Writer objects and initialize them
             *
             * Return RTP_OK on success
             * Return RTP_MEMORY_ERROR if allocation failed
             *
             * Other error return codes are defined in {conn,writer,reader}.hh */
            rtp_error_t init(std::shared_ptr<uvgrtp::zrtp> zrtp);

            /* Initialize Secure RTP session with automatic ZRTP negotiation
             * Allocate Connection/Reader/Writer objects and initialize them
             *
             * Return RTP_OK on success
             * Return RTP_MEMORY_ERROR if allocation failed
             *
             * TODO document all error codes!
             *
             * Other error return codes are defined in {conn,writer,reader,srtp}.hh */
            rtp_error_t init_auto_zrtp(std::shared_ptr<uvgrtp::zrtp> zrtp);
            /// \endcond

            /**
             *
             * \brief Start the ZRTP negotiation manually. 
             *
             * \details There is two ways to use ZRTP.
             * 1. Use flags RCE_SRTP + RCE_SRTP_KMNGMNT_ZRTP + (RCE_ZRTP_DIFFIE_HELLMAN_MODE/RCE_ZRTP_MULTISTREAM_MODE)
             *    to automatically start ZRTP negotiation when creating a media stream.
             * 2. Use flags RCE_SRTP + (RCE_ZRTP_DIFFIE_HELLMAN_MODE/RCE_ZRTP_MULTISTREAM_MODE) and after creating
             *    the media stream, call start_zrtp() to manually start the ZRTP negotiation
             *
             * \return RTP error code
             *
             * \retval  RTP_OK On success
             * \retval  RTP_TIMEOUT if ZRTP timed out
             * \retval  RTP_GENERIC_ERROR on other errors */
            rtp_error_t start_zrtp();

            /**
             *
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
             * \retval  RTP_OK On success
             * \retval  RTP_INVALID_VALUE If key or salt is invalid
             * \retval  RTP_NOT_SUPPORTED If user-managed SRTP was not specified in create_stream() */
            rtp_error_t add_srtp_ctx(uint8_t *key, uint8_t *salt);

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
             * \param data Pointer to data the that should be sent, uvgRTP does not take ownership of the memory
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
            rtp_error_t push_frame(uint8_t *data, size_t data_len, int rtp_flags);

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
             * \param data Pointer to data the that should be sent, uvgRTP does not take ownership of the memory
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
            rtp_error_t push_frame(uint8_t *data, size_t data_len, uint32_t ts, int rtp_flags);

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
            * \param data Pointer to data the that should be sent, uvgRTP does not take ownership of the memory
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
            rtp_error_t push_frame(uint8_t* data, size_t data_len, uint32_t ts, uint64_t ntp_ts, int rtp_flags);

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

            // Disabled for now
            //rtp_error_t push_user_packet(uint8_t* data, uint32_t len);
            //rtp_error_t install_user_receive_hook(void* arg, void (*hook)(void*, uint8_t* data, uint32_t len));
            
            /**
             * \brief Poll a frame indefinitely from the media stream object
             *
             * \return RTP frame
             *
             * \retval uvgrtp::frame::rtp_frame* On success
             * \retval nullptr If an unrecoverable error happened
             */
            uvgrtp::frame::rtp_frame *pull_frame();

            /**
             * \brief Poll a frame for a specified time from the media stream object
             *
             * \param timeout_ms How long is a frame waited, in milliseconds
             *
             * \return RTP frame
             *
             * \retval uvgrtp::frame::rtp_frame* On success
             * \retval nullptr If a frame was not received within the specified time limit or in case of an error
             */
            uvgrtp::frame::rtp_frame *pull_frame(size_t timeout_ms);

            /**
             * \brief Asynchronous way of getting frames
             *
             * \details Receive hook is an alternative to polling frames using uvgrtp::media_stream::pull_frame().
             * Instead of application asking from uvgRTP if there are any new frames available, uvgRTP will notify
             * the application when a frame has been received
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
             * \retval RTP_INVALID_VALUE If hook is nullptr */
            rtp_error_t install_receive_hook(void *arg, void (*hook)(void *, uvgrtp::frame::rtp_frame *));

            /**
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
             * \brief Get the values associated with configuration flags, see ::RTP_CTX_CONFIGURATION_FLAGS for more details
             *
             * \return Value of the configuration flag
             *
             * \retval int value on success
             * \retval -1 on error
             */
            int get_configuration_value(int rcc_flag);

            /// \cond DO_NOT_DOCUMENT

            /* Get unique key of the media stream
             * Used by session to index media streams */
            uint32_t get_key() const;

            /// \endcond

            /**
             * \brief Get pointer to the RTCP object of the media stream
             *
             * \details This object is used to control all RTCP-related functionality
             * and RTCP documentation can be found from \ref uvgrtp::rtcp
             *
             * \return Pointer to RTCP object
             *
             * \retval uvgrtp::rtcp* If RTCP has been enabled (RCE_RTCP has been given to uvgrtp::session::create_stream())
             * \retval nullptr        If RTCP has not been enabled
             */
            uvgrtp::rtcp *get_rtcp();

            /**
             * \brief Get SSRC identifier. You can use the SSRC value for example to find the report 
             * block belonging to this media_stream in RTCP sender/receiver report.
             *
             * \return SSRC value
             */
            uint32_t get_ssrc() const;

        private:
            /* Initialize the connection by initializing the socket
             * and binding ourselves to specified interface and creating
             * an outgoing address */
            rtp_error_t init_connection();

            /* Create the media object for the stream */
            rtp_error_t create_media(rtp_format_t fmt);

            /* free all allocated resources */
            rtp_error_t free_resources(rtp_error_t ret);

            rtp_error_t init_srtp_with_zrtp(int rce_flags, int type, std::shared_ptr<uvgrtp::base_srtp> srtp,
                                            std::shared_ptr<uvgrtp::zrtp> zrtp);

            rtp_error_t start_components();

            rtp_error_t install_packet_handlers();

            uint32_t get_default_bandwidth_kbps(rtp_format_t fmt);

            bool check_pull_preconditions();
            rtp_error_t check_push_preconditions(int rtp_flags, bool smart_pointer);

            inline uint8_t* copy_frame(uint8_t* original, size_t data_len);

            uint32_t key_;

            std::shared_ptr<uvgrtp::srtp>   srtp_;
            std::shared_ptr<uvgrtp::srtcp>  srtcp_;
            std::shared_ptr<uvgrtp::socket> socket_;
            std::shared_ptr<uvgrtp::rtp>    rtp_;
            std::shared_ptr<uvgrtp::rtcp>   rtcp_;
            std::shared_ptr<uvgrtp::zrtp>   zrtp_;

            std::shared_ptr<uvgrtp::socketfactory> sfp_;

            sockaddr_in remote_sockaddr_;
            sockaddr_in6 remote_sockaddr_ip6_;
            std::string remote_address_;
            std::string local_address_;
            uint16_t src_port_;
            uint16_t dst_port_;
            bool ipv6_;
            rtp_format_t fmt_;
            bool new_socket_;

            /* Media context config */
            int rce_flags_ = 0;

            /* Has the media stream been initialized */
            bool initialized_;

            /* RTP packet reception flow. Dispatches packets to other components */
            std::shared_ptr<uvgrtp::reception_flow> reception_flow_;

            /* Media object associated with this media stream. */
            std::unique_ptr<uvgrtp::formats::media> media_;

            /* Thread that keeps the holepunched connection open for unidirectional streams */
            std::unique_ptr<uvgrtp::holepuncher> holepuncher_;

            std::string cname_;

            ssize_t fps_numerator_ = 30;
            ssize_t fps_denominator_ = 1;
            uint32_t bandwidth_ = 0;
            std::shared_ptr<std::atomic<std::uint32_t>> ssrc_;
            std::shared_ptr<std::atomic<std::uint32_t>> remote_ssrc_;

            // Save values associated with context flags, to be returned with get_configuration_value
            // Values are initialized to -2, which means value not set
            int snd_buf_size_;
            int rcv_buf_size_;
    };
}

namespace uvg_rtp = uvgrtp;
