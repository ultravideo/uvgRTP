#pragma once

#include "dispatch.hh"
#include "queue.hh"
#include "socket.hh"

namespace kvz_rtp {

    class sender {
        public:
            sender(kvz_rtp::socket& socket, rtp_ctx_conf& conf, rtp_format_t fmt);
            ~sender();

            rtp_error_t init();
            rtp_error_t destroy();

            /* Split "data" into 1500 byte chunks and send them to remote
             *
             * NOTE: If SCD has been enabled, calling this version of push_frame()
             * requires either that the caller has given a deallocation callback to
             * SCD OR that "flags" contains flags "RTP_COPY"
             *
             * Return RTP_OK success
             * Return RTP_INVALID_VALUE if one of the parameters are invalid
             * Return RTP_MEMORY_ERROR  if the data chunk is too large to be processed
             * Return RTP_SEND_ERROR    if kvzRTP failed to send the data to remote
             * Return RTP_GENERIC_ERROR for any other error condition */
            rtp_error_t push_frame(uint8_t *data, size_t data_len, int flags);

            /* Same as push_frame() defined above but no callback nor RTP_COPY must be provided
             * One must call this like: push_frame(std::move(data), ...) to give ownership of the
             * memory to kvzRTP */
            rtp_error_t push_frame(std::unique_ptr<uint8_t[]> data, size_t data_len, int flags);

        private:
            kvz_rtp::socket socket_;
            rtp_ctx_conf conf_;
            rtp_format_t fmt_;

            sockaddr_in addr_out_;

            kvz_rtp::frame_queue *fqueue_;
            kvz_rtp::dispatcher  *dispatcher_;
    };
};
