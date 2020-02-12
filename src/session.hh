#pragma once

#include <string>
#include <vector>

#include "media_stream.hh"
#include "zrtp.hh"

namespace kvz_rtp {

    class session {
        public:
            session(std::string addr);
            ~session();

            /*
             *
             * TODO
             * TODO
             * TODO
             * TODO
             *
             * Return RTP_OK on success */
            kvz_rtp::media_stream *create_stream(int src_port, int dst_port, rtp_format_t fmt, int flags);

            /* TODO:  */
            kvz_rtp::media_stream *create_receive_stream(int src_port, rtp_format_t fmt, int flags);
            kvz_rtp::media_stream *create_send_stream(int dst_port, rtp_format_t fmt, int flags);

            /* TODO:  */
            rtp_error_t destroy_media_stream(kvz_rtp::media_stream *stream);

        private:
            /* Each RTP multimedia session shall have one ZRTP session from which all session are derived */
            kvz_rtp::zrtp zrtp_;

            /* Each RTP multimedia session is always IP-specific */
            std::string addr_;

            /* All media streams of this session */
            std::unordered_map<uint32_t, kvz_rtp::media_stream *> streams_;
    };
};
