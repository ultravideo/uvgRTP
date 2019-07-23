#pragma once

#include <vector>

#include "conn.hh"
#include "util.hh"

namespace kvz_rtp {

    const int MAX_MSG_COUNT   = 2500;
    const int MAX_CHUNK_COUNT = 5000;

    class frame_queue {
    public:
        frame_queue();
        ~frame_queue();

        /* Cache "header" and "payload" to frame queue
         * Header is placed first in the queue and then the payload
         *
         * Return RTP_OK on success
         * Return RTP_INVALID_VALUE if one of the parameters is invalid
         * Return RTP_MEMORY_ERROR if the maximum amount of chunks/messages is exceeded */
        rtp_error_t enqueue_message(
            kvz_rtp::connection *conn,
            uint8_t *header,  size_t header_len,
            uint8_t *payload, size_t payload_len
        );

        /* Cache "message" to frame queue
         * If this is the first message to be cached, is should be an RTP header
         *
         * Return RTP_OK on success
         * Return RTP_INVALID_VALUE if one of the parameters is invalid
         * Return RTP_MEMORY_ERROR if the maximum amount of chunks/messages is exceeded */
        rtp_error_t enqueue_message(
            kvz_rtp::connection *conn,
            uint8_t *message, size_t message_len
        );

        /* Cache all messages in "buffers" in order to frame queue
         * If this is the first message to be cached, the first buffer
         * of "buffers" should point to an RTP header
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

        /* Set all pointers to 0 and free any memory */
        rtp_error_t empty_queue();

    private:
#ifdef __linux__
    struct mmsghdr headers_[MAX_MSG_COUNT];
    struct msghdr  messages_[MAX_MSG_COUNT];
    struct iovec   chunks_[MAX_CHUNK_COUNT];

    int hdr_ptr_;
    int msg_ptr_;
    int chunk_ptr_;

    sockaddr_in out_addr_;
#else
    std::vector<uint8_t *> merge_bufs_;
    WSABUF buffers_[MAX_MSG_COUNT];
    int buf_ptr_;
#endif
    };
};
