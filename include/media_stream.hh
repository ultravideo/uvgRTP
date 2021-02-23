#pragma once

#include <unordered_map>
#include <memory>

#include "holepuncher.hh"
#include "pkt_dispatch.hh"
#include "rtcp.hh"
#include "socket.hh"
#include "srtp/srtcp.hh"
#include "srtp/srtp.hh"
#include "util.hh"

#include "formats/media.hh"

namespace uvgrtp {

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

            /* Add key for user-managed SRTP session
             *
             * For user-managed SRTP session, the media stream is not started
             * until SRTP key has been added and all calls to push_frame() will fail
             *
             * Currently uvgRTP only supports key length of 16 bytes (128 bits)
             * and salt length of 14 bytes (112 bits).
             * If the key/salt is longer, it is implicitly truncated to correct length
             * and if the key/salt is shorter a memory violation may occur
             *
             * Return RTP_OK on success
             * Return RTP_INVALID_VALUE if "key" or "salt" is invalid
             * Return RTP_NOT_SUPPORTED if user-managed SRTP was not specified in create_stream() */
            rtp_error_t add_srtp_ctx(uint8_t *key, uint8_t *salt);

            /** Split "data" into 1500 byte chunks and send them to remote
             *
             * NOTE: If SCD has been enabled, calling this version of push_frame()
             * requires either that the caller has given a deallocation callback to
             * SCD OR that "flags" contains flags "RTP_COPY"
             *
             * NOTE: Each push_frame() sends one discrete frame of data. If the input frame
             * is fragmented, calling application should call push_frame() with RTP_MORE
             * and RTP_SLICE flags to prevent uvgRTP from flushing the frame queue after
             * push_frame().
             *
             * push_frame(..., RTP_MORE | RTP_SLICE); // more data coming in, do not flush queue
             * push_frame(..., RTP_MORE | RTP_SLICE); // more data coming in, do not flush queue
             * push_frame(..., RTP_SLICE);            // no more data coming in, flush queue
             *
             * If user wishes to manage RTP timestamps himself, he may pass "ts" to push_frame()
             * which forces uvgRTP to use that timestamp for all RTP packets of "data".
             *
             * Return RTP_OK success
             * Return RTP_INVALID_VALUE if one of the parameters are invalid
             * Return RTP_MEMORY_ERROR  if the data chunk is too large to be processed
             * Return RTP_SEND_ERROR    if uvgRTP failed to send the data to remote
             * Return RTP_GENERIC_ERROR for any other error condition */
            rtp_error_t push_frame(uint8_t *data, size_t data_len, int flags);
            rtp_error_t push_frame(uint8_t *data, size_t data_len, uint32_t ts, int flags);
            rtp_error_t push_frame(std::unique_ptr<uint8_t[]> data, size_t data_len, int flags);
            rtp_error_t push_frame(std::unique_ptr<uint8_t[]> data, size_t data_len, uint32_t ts, int flags);

            /**
             * \brief Poll a frame indefinetily from the media stream object
             *
             * When a frame is received, it is put into the frame vector of the receiver
             * Calling application can poll frames by calling pull_frame().
             *
             * NOTE: pull_frame() is a blocking operation and a separate thread should be
             * spawned for it!
             *
             * You can specify for how long should pull_frame() block by giving "timeout"
             * parameter that denotes how long pull_frame() will wait for an incoming frame
             * in milliseconds
             *
             * Return pointer to RTP frame on success */
            uvgrtp::frame::rtp_frame *pull_frame();

            /**
             * \brief Poll a frame for a specified time from the media stream object
             *
             * When a frame is received, it is put into the frame vector of the receiver
             * Calling application can poll frames by calling pull_frame().
             *
             * NOTE: pull_frame() is a blocking operation and a separate thread should be
             * spawned for it!
             *
             * You can specify for how long should pull_frame() block by giving "timeout"
             * parameter that denotes how long pull_frame() will wait for an incoming frame
             * in milliseconds
             *
             * Return pointer to RTP frame on success */
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
            std::thread *dispatcher_thread_;

            /* Media object associated with this media stream. */
            uvgrtp::formats::media *media_;

            /* Thread that keeps the holepunched connection open for unidirectional streams */
            uvgrtp::holepuncher *holepuncher_;
    };
};

namespace uvg_rtp = uvgrtp;
