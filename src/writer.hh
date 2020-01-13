#pragma once

#include <stdint.h>

#include "conn.hh"
#include "frame.hh"
#include "queue.hh"
#include "socket.hh"

namespace kvz_rtp {

    class writer : public connection {
        public:
            /* "src_port" is an optional argument, given if holepunching want to be used */
            writer(rtp_format_t fmt, std::string dst_addr, int dst_port);
            writer(rtp_format_t fmt, std::string dst_addr, int dst_port, int src_port);
            ~writer();

            /* Open socket for sending frames and start SCD if enabled
             * Start RTCP instance if not already started
             *
             * Return RTP_OK on success
             * Return RTP_SOCKET_ERROR if the socket couldn't be initialized
             * Return RTP_BIND_ERROR   if binding to src_port_ failed
             * Return RTP_MEMORY_ERROR if RTCP instance couldn't be allocated
             * Return RTP_GENERIC_ERROR for any other error condition */
            rtp_error_t start();

            /* Stop and destroy SCD if it's enabled
             * stop() will block until SCD is finished to ensure that all packet are sent to remote 
             *
             * Return RTP_OK on success */
            rtp_error_t stop();

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

            /* TODO: remove */
            sockaddr_in get_out_address();

        private:
            std::string dst_addr_;
            int dst_port_;
            int src_port_;
            sockaddr_in addr_out_;

            kvz_rtp::frame_queue *fqueue_;
            kvz_rtp::dispatcher  *dispatcher_;
    };
};
