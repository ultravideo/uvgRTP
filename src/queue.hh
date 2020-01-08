#pragma once

#include <atomic>
#include <memory>
#include <unordered_map>
#include <vector>

#include "conn.hh"
#include "dispatch.hh"
#include "util.hh"

const int MAX_MSG_COUNT   = 500;
const int MAX_QUEUED_MSGS =  10;
const int MAX_CHUNK_COUNT =   4;

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
        kvz_rtp::frame::rtp_header *rtp_headers;

#ifdef __linux__
        struct mmsghdr *headers;
        struct iovec   *chunks;
#else
        char *headers;
        char *chunks;
#endif

        /* Media may need space for additional buffers,
         * this pointer is initialized with kvz_rtp::MEDIA_TYPE::media_headers
         * when the transaction is initialized for the first time
         *
         * See src/formats/hevc.hh for example */
        void *media_headers;

        size_t chunk_ptr;
        size_t hdr_ptr;
        size_t rtphdr_ptr;

        /* Address of receiver, used by sendmmsg(2) */
        sockaddr_in out_addr;

        /* Used by the system call dispatcher for transaction deinitialization */
        kvz_rtp::frame_queue *fqueue;

        /* If SCD is used, it's absolutely essential to initialize transaction
         * by giving the data pointer to frame queue
         *
         * When the SCD has processed the transaction,
         * it will be destroyed freeing the "data_smart" automatically.
         *
         * If "data_raw" is set instead, the SCD checks if a deallocation callback is provided
         * and if so, it will deallocate the memory using the callback
         *
         * If callback is not provided, SCD will check if "flags" field contains the flag "RTP_COPY"
         * which means that kvzRTP has a made a copy of the original chunk and it can be safely freed */
        std::unique_ptr<uint8_t[]> data_smart;
        uint8_t *data_raw;

        /* If the application code provided us a deallocation hook, this points to it.
         * When SCD finishes processing a transaction, it will call this hook with "data_raw" pointer */
        void (*dealloc_hook)(void *);

    } transaction_t;

    class frame_queue {
        public:
            frame_queue(rtp_format_t fmt, rtp_ctx_conf_t& conf);
            frame_queue(rtp_format_t fmt, rtp_ctx_conf_t& conf, kvz_rtp::dispatcher *dispatcher);
            ~frame_queue();

            rtp_error_t init_transaction(kvz_rtp::connection *conn);
            rtp_error_t init_transaction(kvz_rtp::connection *conn, uint8_t *data);
            rtp_error_t init_transaction(kvz_rtp::connection *conn, std::unique_ptr<uint8_t[]> data);

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

            /* Because frame queue supports both raw and smart pointers and the smart pointer ownership
             * is transferred to active transaction, the code that created the transaction must query
             * the data pointer from frame queue explicitly
             *
             * Return raw data pointer for caller on success
             * Return nullptr if no data pointer is set or if there is no active transaction */
            uint8_t *get_active_dataptr();

            /* Install deallocation hook for external memory chunks
             *
             * When user doesn't issue RTP_COPY and doesn't pass unique_ptr, memory given
             * to push_frame() must be deallocated manually. kvzRTP doesn't know the memory
             * type (or whether can be even deallocated) so application must provide a way
             * to deallocate the chunk
             *
             * If raw pointer without RTP_COPY is given to push_frame() and the deallocation
             * hook is missing, kvzRTP won't do anything about the memory which can lead to
             * significant memory leaks */
            void install_dealloc_hook(void (*dealloc_hook)(void *));

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

            /* Set to nullptr if this frame queue doesn't use dispatcher */
            kvz_rtp::dispatcher *dispatcher_;

            /* Deallocation hook is stored here and copied to transaction upon initialization */
            void (*dealloc_hook_)(void *);

            ssize_t max_queued_; /* number of queued transactions */
            ssize_t max_mcount_; /* number of messages per transactions */
            ssize_t max_ccount_; /* number of chunks per message */
    };
};
