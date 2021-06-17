#pragma once

#include "util.hh"

#include <unordered_map>
#include <memory>
#include <string>


#ifndef _WIN32
#include <sys/socket.h>
#include <netinet/in.h>
#endif

namespace uvgrtp {

    // forward declarations
    class rtp;
    class rtcp;

    class zrtp;
    class base_srtp;
    class srtp;
    class srtcp;

    class pkt_dispatcher;
    class holepuncher;
    class socket;

    namespace frame {
        struct rtp_frame;
    };

    namespace formats {
        class media;
    };

    class media_stream {
        public:
            /// \cond DO_NOT_DOCUMENT
            media_stream(std::string addr, int src_port, int dst_port, rtp_format_t fmt, int flags);
            media_stream(std::string remote_addr, std::string local_addr, int src_port, int dst_port, rtp_format_t fmt, int flags);
            ~media_stream();

            /* Initialize traditional RTP session
             * Allocate Connection/Reader/Writer objects and initialize them
             *
             * Return RTP_OK on success
             * Return RTP_MEMORY_ERROR if allocation failed
             *
             * Other error return codes are defined in {conn,writer,reader}.hh */
            rtp_error_t init();

            /* Initialize Secure RTP session
             * Allocate Connection/Reader/Writer objects and initialize them
             *
             * Return RTP_OK on success
             * Return RTP_MEMORY_ERROR if allocation failed
             *
             * TODO document all error codes!
             *
             * Other error return codes are defined in {conn,writer,reader,srtp}.hh */
            rtp_error_t init(uvgrtp::zrtp *zrtp);
            /// \endcond

            /**
             *
             * \brief Add keying information for user-managed SRTP session
             *
             * \details For user-managed SRTP session, the media stream is not started
             * until SRTP key has been added and all calls to push_frame() will fail
             *
             * Notice that if user-managed SRTP has been enabled during media stream creation,
             * this function must be called before anything else. All calls to other functions
             * will fail with ::RTP_NOT_INITIALIZED until the SRTP context has been specified
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
             * ::RTP_CTX_ENABLE_FLAGS, uvgRTP fragments the input data into RTP packets of 1500 bytes,
             * or to any other size defined by the application using ::RCC_MTU_SIZE
             *
             * The frame is automatically reconstructed by the receiver if all fragments have been
             * received successfully.
             *
             * \param data Pointer to data the that should be sent
             * \param data_len Length of data
             * \param flags Optional flags, see ::RTP_FLAGS for more details
             *
             * \return RTP error code
             *
             * \retval  RTP_OK            On success
             * \retval  RTP_INVALID_VALUE If one of the parameters are invalid
             * \retval  RTP_MEMORY_ERROR  If the data chunk is too large to be processed
             * \retval  RTP_SEND_ERROR    If uvgRTP failed to send the data to remote
             * \retval  RTP_GENERIC_ERROR If an unspecified error occurred
             */
            rtp_error_t push_frame(uint8_t *data, size_t data_len, int flags);

            /**
             * \brief Send data to remote participant with a custom timestamp
             *
             * \details If so specified either by the selected media format and/or given
             * ::RTP_CTX_ENABLE_FLAGS, uvgRTP fragments the input data into RTP packets of 1500 bytes,
             * or to any other size defined by the application using ::RCC_MTU_SIZE
             *
             * The frame is automatically reconstructed by the receiver if all fragments have been
             * received successfully.
             *
             * \param data Smart pointer to data the that should be sent
             * \param data_len Length of data
             * \param flags Optional flags, see ::RTP_FLAGS for more details
             *
             * \return RTP error code
             *
             * \retval  RTP_OK            On success
             * \retval  RTP_INVALID_VALUE If one of the parameters are invalid
             * \retval  RTP_MEMORY_ERROR  If the data chunk is too large to be processed
             * \retval  RTP_SEND_ERROR    If uvgRTP failed to send the data to remote
             * \retval  RTP_GENERIC_ERROR If an unspecified error occurred
             */
            rtp_error_t push_frame(std::unique_ptr<uint8_t[]> data, size_t data_len, int flags);

            /**
             * \brief Send data to remote participant with a custom timestamp
             *
             * \details If so specified either by the selected media format and/or given
             * ::RTP_CTX_ENABLE_FLAGS, uvgRTP fragments the input data into RTP packets of 1500 bytes,
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
             * \param data Pointer to data the that should be sent
             * \param data_len Length of data
             * \param ts 32-bit timestamp value for the data
             * \param flags Optional flags, see ::RTP_FLAGS for more details
             *
             * \return RTP error code
             *
             * \retval  RTP_OK            On success
             * \retval  RTP_INVALID_VALUE If one of the parameters are invalid
             * \retval  RTP_MEMORY_ERROR  If the data chunk is too large to be processed
             * \retval  RTP_SEND_ERROR    If uvgRTP failed to send the data to remote
             * \retval  RTP_GENERIC_ERROR If an unspecified error occurred
             */
            rtp_error_t push_frame(uint8_t *data, size_t data_len, uint32_t ts, int flags);

            /**
             * \brief Send data to remote participant with a custom timestamp
             *
             * \details If so specified either by the selected media format and/or given
             * ::RTP_CTX_ENABLE_FLAGS, uvgRTP fragments the input data into RTP packets of 1500 bytes,
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
             * \param flags Optional flags, see ::RTP_FLAGS for more details
             *
             * \return RTP error code
             *
             * \retval  RTP_OK            On success
             * \retval  RTP_INVALID_VALUE If one of the parameters are invalid
             * \retval  RTP_MEMORY_ERROR  If the data chunk is too large to be processed
             * \retval  RTP_SEND_ERROR    If uvgRTP failed to send the data to remote
             * \retval  RTP_GENERIC_ERROR If an unspecified error occurred
             */
            rtp_error_t push_frame(std::unique_ptr<uint8_t[]> data, size_t data_len, uint32_t ts, int flags);

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
             * \param timeout How long is a frame waited, in milliseconds
             *
             * \return RTP frame
             *
             * \retval uvgrtp::frame::rtp_frame* On success
             * \retval nullptr If a frame was not received within the specified time limit
             * \retval nullptr If an unrecoverable error happened
             */
            uvgrtp::frame::rtp_frame *pull_frame(size_t timeout);

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

            /// \cond DO_NOT_DOCUMENT
            /* If system call dispatcher is enabled and calling application has special requirements
             * for the deallocation of a frame, it may install a deallocation hook which is called
             * when SCD has processed the frame
             *
             * Return RTP_OK on success
             * Return RTP_INVALID_VALUE if "hook" is nullptr */
            rtp_error_t install_deallocation_hook(void (*hook)(void *));

            /* If needed, a notification hook can be installed to uvgRTP that can be used as
             * an information side channel to the internal state of the library.
             *
             * When uvgRTP encouters a situation it doesn't know how to react to,
             * it calls the notify hook with certain notify reason number (src/util.hh).
             * Upon receiving a notification, application may ignore it or act on it somehow
             *
             * Currently only one notification type is supported and only receiver uses notifications
             *
             * "arg" is optional argument that is passed to hook when it is called. It may be nullptr
             *
             * Return RTP_OK on success
             * Return RTP_INVALID_VALUE if "hook" is nullptr */
            rtp_error_t install_notify_hook(void *arg, void (*hook)(void *, int));
            /// \endcond

            /**
             * \brief Configure the media stream, see ::RTP_CTX_CONFIGURATION_FLAGS for more details
             *
             * \return RTP error code
             *
             * \retval RTP_OK On success
             * \retval RTP_INVALID_VALUE If the provided value is not valid for a given configuration flag
             * \retval RTP_INVALID_VALUE If the provided configuration flag is not supported
             * \retval RTP_GENERIC_ERROR If setsockopt(2) failed
             */
            rtp_error_t configure_ctx(int flag, ssize_t value);

            /// \cond DO_NOT_DOCUMENT
            /* Setter and getter for media-specific config that can be used f.ex with Opus */
            void  set_media_config(void *config);
            void *get_media_config();

            /* Get unique key of the media stream
             * Used by session to index media streams */
            uint32_t get_key();
            /// \endcond

            /**
             *
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

        private:
            /* Initialize the connection by initializing the socket
             * and binding ourselves to specified interface and creating
             * an outgoing address */
            rtp_error_t init_connection();

            /* Create the media object for the stream */
            rtp_error_t create_media(rtp_format_t fmt);

            /* free all allocated resources */
            rtp_error_t free_resources(rtp_error_t ret);

            rtp_error_t init_srtp_with_zrtp(int flags, int type, uvgrtp::base_srtp* srtp,
                                            uvgrtp::zrtp *zrtp);

            uint32_t key_;

            uvgrtp::srtp   *srtp_;
            uvgrtp::srtcp  *srtcp_;
            uvgrtp::socket *socket_;
            uvgrtp::rtp    *rtp_;
            uvgrtp::rtcp   *rtcp_;

            sockaddr_in addr_out_;
            std::string addr_;
            std::string laddr_;
            int src_port_;
            int dst_port_;
            rtp_format_t fmt_;
            int flags_;

            /* Media context config (SCD etc.) */
            rtp_ctx_conf_t ctx_config_;

            /* Media config f.ex. for Opus */
            void *media_config_;

            /* Has the media stream been initialized */
            bool initialized_;

            /* Primary handler keys for the RTP packet dispatcher */
            uint32_t rtp_handler_key_;
            uint32_t zrtp_handler_key_;

            /* RTP packet dispatcher for the receiver */
            uvgrtp::pkt_dispatcher *pkt_dispatcher_;

            /* Media object associated with this media stream. */
            uvgrtp::formats::media *media_;

            /* Thread that keeps the holepunched connection open for unidirectional streams */
            uvgrtp::holepuncher *holepuncher_;
    };
};

namespace uvg_rtp = uvgrtp;
