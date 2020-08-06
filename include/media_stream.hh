#pragma once

#include <unordered_map>
#include <memory>

#include "pkt_dispatch.hh"
#include "rtcp.hh"
#include "socket.hh"
#include "srtp.hh"
#include "util.hh"

#include "formats/media.hh"

namespace uvg_rtp {

    enum mstream_type {
        BIDIRECTIONAL,
        UNIDIRECTIONAL_SENDER,
        UNIDIRECTIONAL_RECEIVER
    };

    class media_stream {
        public:
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
#ifdef __RTP_CRYPTO__
            rtp_error_t init(uvg_rtp::zrtp *zrtp);

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
#endif

            /* Split "data" into 1500 byte chunks and send them to remote
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

            /* When a frame is received, it is put into the frame vector of the receiver
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
            uvg_rtp::frame::rtp_frame *pull_frame();
            uvg_rtp::frame::rtp_frame *pull_frame(size_t timeout);

            /* Alternative to pull_frame(). The provided hook is called when a frame is received.
             *
             * "arg" is optional argument that is passed to hook when it is called. It may be nullptr
             *
             * NOTE: Hook should not be used to process the frame but it should be a place where the
             * frame handout happens from uvgRTP to application
             *
             * Return RTP_OK on success
             * Return RTP_INVALID_VALUE if "hook" is nullptr */
            rtp_error_t install_receive_hook(void *arg, void (*hook)(void *, uvg_rtp::frame::rtp_frame *));

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

            /* Configure the media stream in various ways
             *
             * See utils.hh for more details
             *
             * Return RTP_OK on success
             * Return RTP_INVALID_VALUE if "flag" is not recognized or "value" is invalid */
            rtp_error_t configure_ctx(int flag, ssize_t value);

            /* Setter and getter for media-specific config that can be used f.ex with Opus */
            void  set_media_config(void *config);
            void *get_media_config();

            /* Overwrite the payload type set during initialization */
            rtp_error_t set_dynamic_payload(uint8_t payload);

            /* Get unique key of the media stream
             * Used by session to index media streams */
            uint32_t get_key();

            /* Get pointer to the RTCP object of the media stream
             *
             * This object is used to control all RTCP-related functionality
             * and RTCP documentation can be found from include/rtcp.hh
             *
             * Return pointer to RTCP object on success
             * Return nullptr if RTCP has been created */
            uvg_rtp::rtcp *get_rtcp();

        private:
            /* Initialize the connection by initializing the socket
             * and binding ourselves to specified interface and creating
             * an outgoing address */
            rtp_error_t init_connection();

            uint32_t key_;

            uvg_rtp::srtp       *srtp_;
            uvg_rtp::socket     socket_;
            uvg_rtp::rtp        *rtp_;
            uvg_rtp::rtcp       *rtcp_;

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

            /* media stream type */
            enum mstream_type type_;

            /* Primary handler's key for the RTP packet dispatcher */
            uint32_t rtp_handler_key_;

            /* RTP packet dispatcher for the receiver */
            uvg_rtp::pkt_dispatcher *pkt_dispatcher_;
            std::thread *dispatcher_thread_;

            /* Media object associated with this media stream. */
            uvg_rtp::formats::media *media_;
    };
};
