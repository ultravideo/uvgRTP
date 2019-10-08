#pragma once

#include <atomic>
#include <unordered_map>
#include <vector>

#include "conn.hh"
#include "dispatch.hh"
#include "util.hh"

#ifdef __RTP_FQUEUE_RING_BUFFER_SIZE__
#   define MAX_MSG_COUNT __RTP_FQUEUE_RING_BUFFER_SIZE__
#else
#   define MAX_MSG_COUNT 1500
#endif

#ifdef __RTP_FQUEUE_RING_BUFFER_BUFFS_PER_PACKET__
#   define MAX_CHUNK_COUNT (MAX_MSG_COUNT * __RTP_FQUEUE_RING_BUFFER_BUFFS_PER_PACKET__)
#else
#   define MAX_CHUNK_COUNT (MAX_MSG_COUNT * 4)
#endif

#define MAX_QUEUED_MSGS 20

namespace kvz_rtp {

    typedef struct active_range {
        size_t h_start; size_t h_end;
        size_t c_start; size_t c_end;
    } active_t;

    typedef std::vector<std::pair<size_t, uint8_t *>> buff_vec;

    typedef struct transaction {
        /* Each transaction has a unique key which is used by the SCD (if enabled)
         * when moving the transactions betwen "queued_" and "free_" */
        uint32_t key;

        /* To provide true scatter/gather I/O, each transaction has a buff_vec
         * structure which may contain differents buffers and their sizes.
         *
         * This can be used, for example, for storing media-specific headers
         * which are then passed (along with the actual media) to enqueue_message() */
        kvz_rtp::buff_vec buffers;

        /* All packets of a transaction share the common RTP header only differing in sequence number.
         * Keeping a separate common RTP header and then just copying this is cleaner than initializing
         * RTP header for each packet */
        kvz_rtp::frame::rtp_header rtp_common;
        kvz_rtp::frame::rtp_header rtp_headers[MAX_MSG_COUNT];

        struct mmsghdr headers[MAX_MSG_COUNT];
        struct iovec   chunks[MAX_CHUNK_COUNT];

        /* Media may need space for additional buffers,
         * this pointer is initialized with kvz_rtp::MEDIA_TYPE::media_headers
         * when the transaction is initialized for the first time
         *
         * See src/formats/hevc.hh for example */
        void *media_headers;

        size_t chunk_ptr;
        size_t hdr_ptr;
        size_t rtphdr_ptr;

        sockaddr_in out_addr;
    } transaction_t;

    class frame_queue {
        public:
            frame_queue(rtp_format_t fmt);
            ~frame_queue();

            rtp_error_t init_transaction(kvz_rtp::connection *conn);

            /* If there are less than "MAX_QUEUED_MSGS" in the "free_" vector,
             * the transaction is moved there, otherwise it's destroyed
             *
             * If parameter "key" is given, the transaction with that key will be deinitialized
             * Otherwise the active transaction is deinitialized
             *
             * Return RTP_OK on success
             * Return RTP_INVALID_VALUE if "key" doesn't point to valid transaction */
            rtp_error_t deinit_transaction();
            rtp_error_t deinit_transaction(uint32_t key);

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
                buff_vec& buffers
            );

            /* Flush the message queue
             *
             * Return RTP_OK on success
             * Return RTP_INVALID_VALUE if "conn" is nullptr or message buffer is empty
             * return RTP_SEND_ERROR if send fails */
            rtp_error_t flush_queue(kvz_rtp::connection *conn);

            /* Media may have extra headers (f.ex. NAL and FU headers for HEVC).
             * These headers must be valid until the message is sent (ie. they cannot be saved to
             * caller's stack).
             *
             * Buff_vec is the place to store these extra headers (see src/formats/hevc.cc) */
            kvz_rtp::buff_vec& get_buffer_vector();

            /* Each media may allocate extra buffers for the transaction struct if need be
             *
             * These headers must be stored in the transaction structure (instead of into
             * caller's stack) because if system call dispatcher is used, the transaction is
             * not committed immediately but rather given to SCD. This means that when SCD
             * starts working on the transaction, the buffers that were on the caller's stack
             * are now invalid and the transaction will fail/garbage will be sent
             *
             * Return pointer to media headers if they're set
             * Return nullptr if they're not set */
            void *get_media_headers();

            /* Update the active task's current packet's sequence number */
            void update_rtp_header(kvz_rtp::connection *conn);

        private:
            /* Both the application and SCD access "free_" and "queued_" structures so the
             * access must be protected by a mutex
             *
             * When application has finished making the transaction, it will push it to "queued_"
             * from which it's moved back to "free_" by the SCD when the time comes. */
            std::mutex transaction_mtx_;
            std::vector<transaction_t *> free_;
            std::unordered_map<uint32_t, transaction_t *> queued_;

            transaction_t *active_;

            /* What is the media format used to send data using this frame queue
             * This is used when allocating new transactions */
            rtp_format_t fmt_;
    };
};
