#pragma once

#include <unordered_map>
#include <memory>

#include "receiver.hh"
#include "rtcp.hh"
#include "sender.hh"
#include "socket.hh"
#include "srtp.hh"
#include "util.hh"

namespace kvz_rtp {

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
            rtp_error_t init(kvz_rtp::zrtp *zrtp);
#endif

            /* Split "data" into 1500 byte chunks and send them to remote
             *
             * NOTE: If SCD has been enabled, calling this version of push_frame()
             * requires either that the caller has given a deallocation callback to
             * SCD OR that "flags" contains flags "RTP_COPY"
             *
             * NOTE: Each push_frame() sends one discrete frame of data. If the input frame
             * is fragmented, calling application should call push_frame() with RTP_MORE
             * and RTP_SLICE flags to prevent kvzRTP from flushing the frame queue after
             * push_frame().
             *
             * push_frame(..., RTP_MORE | RTP_SLICE); // more data coming in, do not flush queue
             * push_frame(..., RTP_MORE | RTP_SLICE); // more data coming in, do not flush queue
             * push_frame(..., RTP_SLICE);            // no more data coming in, flush queue
             *
             * Return RTP_OK success
             * Return RTP_INVALID_VALUE if one of the parameters are invalid
             * Return RTP_MEMORY_ERROR  if the data chunk is too large to be processed
             * Return RTP_SEND_ERROR    if kvzRTP failed to send the data to remote
             * Return RTP_GENERIC_ERROR for any other error condition */
            rtp_error_t push_frame(uint8_t *data, size_t data_len, int flags);
            rtp_error_t push_frame(std::unique_ptr<uint8_t[]> data, size_t data_len, int flags);

            /* When a frame is received, it is put into the frame vector of the receiver
             * Calling application can poll frames by calling pull_frame().
             *
             * NOTE: pull_frame() is a blocking operation and a separate thread should be
             * spawned for it!
             *
             * Return pointer to RTP frame on success */
            kvz_rtp::frame::rtp_frame *pull_frame();

            /* Alternative to pull_frame(). The provided hook is called when a frame is received.
             *
             * "arg" is optional argument that is passed to hook when it is called. It may be nullptr
             *
             * NOTE: Hook should not be used to process the frame but it should be a place where the
             * frame handout happens from kvzRTP to application
             *
             * Return RTP_OK on success
             * Return RTP_INVALID_VALUE if "hook" is nullptr */
            rtp_error_t install_receive_hook(void *arg, void (*hook)(void *, kvz_rtp::frame::rtp_frame *));

            /* If system call dispatcher is enabled and calling application has special requirements
             * for the deallocation of a frame, it may install a deallocation hook which is called
             * when SCD has processed the frame
             *
             * Return RTP_OK on success
             * Return RTP_INVALID_VALUE if "hook" is nullptr */
            rtp_error_t install_deallocation_hook(void (*hook)(void *));

            /* If needed, a notification hook can be installed to kvzRTP that can be used as
             * an information side channel to the internal state of the library.
             *
             * When kvzRTP encouters a situation it doesn't know how to react to,
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
             * The first version takes a configuration flag and value for that configuration (f.ex. UDP buffer size)
             * The second version only takes a flag that enables some functionality (f.ex. SCD)
             *
             * See utils.hh for more details
             *
             * Return RTP_OK on success
             * Return RTP_INVALID_VALUE if "flag" is not recognized or "value" is invalid */
            rtp_error_t configure_ctx(int flag, ssize_t value);
            rtp_error_t configure_ctx(int flag);

            /* Setter and getter for media-specific config that can be used f.ex with Opus */
            void  set_media_config(void *config);
            void *get_media_config();

            /* Get unique key of the media stream
             * Used by session to index media streams */
            uint32_t get_key();

        private:
            /* Initialize the connection by initializing the socket
             * and binding ourselves to specified interface and creating
             * an outgoing address */
            rtp_error_t init_connection();

            uint32_t key_;

            kvz_rtp::srtp       *srtp_;
            kvz_rtp::socket     socket_;
            kvz_rtp::sender     *sender_;
            kvz_rtp::receiver   *receiver_;
            kvz_rtp::rtp        *rtp_;

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
    };
};
