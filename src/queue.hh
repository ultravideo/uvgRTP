#pragma once

#include <vector>

#include "conn.hh"
#include "util.hh"

namespace kvz_rtp {

    const int MAX_MSG_COUNT   = 5000;
    const int MAX_CHUNK_COUNT = 10000;

    class frame_queue {
    public:
        frame_queue();
        ~frame_queue();

        rtp_error_t enqueue_message(
            kvz_rtp::connection *conn,
            uint8_t *header,  size_t header_len,
            uint8_t *payload, size_t payload_len
        );

        rtp_error_t enqueue_message(
            kvz_rtp::connection *conn,
            uint8_t *message, size_t message_len
        );

        rtp_error_t enqueue_message(
            kvz_rtp::connection *conn,
            std::vector<std::pair<size_t, uint8_t *>>& buffers
        );

        rtp_error_t flush_queue(kvz_rtp::connection *conn);

        rtp_error_t empty_queue();

    private:
#ifdef __linux__
    struct mmsghdr headers_[MAX_MSG_COUNT];
    struct msghdr  messages_[MAX_MSG_COUNT];
    struct iovec   chunks_[MAX_CHUNK_COUNT];

    int hdr_ptr_;
    int msg_ptr_;
    int chunk_ptr_;
#else
    /* TODO: winsock */
#endif
    };
};
