#pragma once

#include <unordered_map>
#include <memory>

#include "conn.hh"
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
             * Other error return codes are defined in {conn,writer,reader,srtp}.hh */
            rtp_error_t init(kvz_rtp::zrtp& zrtp);

            /* TODO: all writer-related stuff here with good documentation */
            rtp_error_t push_frame(uint8_t *data, size_t data_len, int flags);
            rtp_error_t push_frame(std::unique_ptr<uint8_t[]> data, size_t data_len, int flags);

            /* TODO: all reader-related stuff here with good documentation */
            kvz_rtp::frame::rtp_frame *pull_frame();

            /* TODO: document */
            rtp_error_t install_receive_hook(void *arg, void (*hook)(void *, kvz_rtp::frame::rtp_frame *));

            /* TODO: document */
            rtp_error_t install_deallocation_hook(void (*hook)(void *));

            /* TODO: document */
            rtp_ctx_conf_t& get_ctx_config();
            rtp_error_t configure_ctx(int flag, ssize_t value);
            rtp_error_t configure_ctx(int flag);

            /* TODO: document */
            void  set_media_config(void *config);
            void *get_media_config();

        private:
            uint32_t key_;

            kvz_rtp::connection *conn_;
            kvz_rtp::sender     *sender_;
            kvz_rtp::receiver   *receiver_;
            kvz_rtp::srtp       *srtp_;
            kvz_rtp::rtp        *rtp_;

            std::string addr_;
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
