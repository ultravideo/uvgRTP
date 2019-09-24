#pragma once

#include <vector>

#include "conn.hh"
#include "util.hh"

namespace kvz_rtp {

    const int MAX_MSG_COUNT   = 1500;
    const int MAX_CHUNK_COUNT = 3000;

    class frame_queue {
    public:
        frame_queue();
        ~frame_queue();

        /* Initialize the RTP Header for fragments and save outgoing address */
        rtp_error_t init_queue(kvz_rtp::connection *conn);

        /* Cache "message" to frame queue
         *
         * Return RTP_OK on success
         * Return RTP_INVALID_VALUE if one of the parameters is invalid
         * Return RTP_MEMORY_ERROR if the maximum amount of chunks/messages is exceeded */
        rtp_error_t enqueue_message(
            kvz_rtp::connection *conn,
            uint8_t *message, size_t message_len
        );

        /* Cache all messages in "buffers" in order to frame queue
         *
         * Return RTP_OK on success
         * Return RTP_INVALID_VALUE if one of the parameters is invalid
         * Return RTP_MEMORY_ERROR if the maximum amount of chunks/messages is exceeded */
        rtp_error_t enqueue_message(
            kvz_rtp::connection *conn,
            std::vector<std::pair<size_t, uint8_t *>>& buffers
        );

        /* Flush the message queue
         *
         * Return RTP_OK on success
         * Return RTP_INVALID_VALUE if "conn" is nullptr or message buffer is empty
         * return RTP_SEND_ERROR if send fails */
        rtp_error_t flush_queue(kvz_rtp::connection *conn);

        /* Set all pointers to 0 and free any memory
         *
         * NOTE: For each init_queue(), there MUST be a call to deinit_queue()
         * Otherwise the queue is left in an uninitialized state */
        rtp_error_t deinit_queue();

    private:
        void update_rtp_header(kvz_rtp::connection *conn);

        kvz_rtp::frame::rtp_header rtpheaders_[MAX_MSG_COUNT];
        kvz_rtp::frame::rtp_frame rtphdr_;

        bool initialized_;
        int rtphdr_ptr_;
        int chunk_ptr_;

        sockaddr_in out_addr_;

#ifdef __linux__
        struct mmsghdr headers_[MAX_MSG_COUNT];
        struct iovec   chunks_[MAX_CHUNK_COUNT];

        int hdr_ptr_;
#else
        WSAMSG messages_[MAX_MSG_COUNT];
        WSABUF buffers_[MAX_CHUNK_COUNT];

        int buf_ptr_;
#endif
    };
};
