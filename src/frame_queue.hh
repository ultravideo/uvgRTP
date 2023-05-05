#pragma once

#include "uvgrtp/frame.hh"
#include "uvgrtp/util.hh"

#include "socket.hh"

#include <atomic>
#include <memory>
#include <unordered_map>
#include <vector>
#include <mutex>

#ifdef _WIN32
#include <ws2def.h>
#include <ws2ipdef.h>
#else
#include <netinet/in.h>
#endif

// TODO: get these from socket?
const int MAX_MSG_COUNT   = 5000;
const int MAX_QUEUED_MSGS =  10;
const int MAX_CHUNK_COUNT =   4;

namespace uvgrtp {
    class rtp;

    typedef struct transaction {

        /* To provide true scatter/gather I/O, each transaction has a buf_vec
         * structure which may contain differents buffers and their sizes.
         *
         * This can be used, for example, for storing media-specific headers
         * which are then passed (along with the actual media) to enqueue_message() */
        uvgrtp::buf_vec buffers;

        /* Each RTP frame of a transaction is constructed using buf_vec structure and
         * each buf_vec structure is pushed to pkt_vec */
        uvgrtp::pkt_vec packets;

        /* All packets of a transaction share the common RTP header only differing in sequence number.
         * Keeping a separate common RTP header and then just copying this is cleaner than initializing
         * RTP header for each packet */
        uvgrtp::frame::rtp_header rtp_common;
        uvgrtp::frame::rtp_header *rtp_headers = nullptr;

#ifndef _WIN32
        struct mmsghdr *headers = nullptr;
        struct iovec   *chunks = nullptr;
#else
        char *headers = nullptr;
        char *chunks = nullptr;
#endif

        /* Media may need space for additional buffers,
         * this pointer is initialized with uvgrtp::MEDIA_TYPE::media_headers
         * when the transaction is initialized for the first time
         *
         * See src/formats/hevc.hh for example */
        void *media_headers = nullptr;

        /* Pointer to RTP authentication (if enabled) */
        uint8_t *rtp_auth_tags = nullptr;

        size_t hdr_ptr = 0;
        size_t rtphdr_ptr = 0;
        size_t rtpauth_ptr = 0;

        /* The flag "RTP_COPY" means that uvgRTP has a made a copy of the original chunk 
         * and it can be safely freed */
        std::unique_ptr<uint8_t[]> data_smart;
        uint8_t *data_raw = nullptr;

        /* If the application code provided us a deallocation hook, this points to it.
         * When SCD finishes processing a transaction, it will call this hook with "data_raw" pointer */
        void (*dealloc_hook)(void *) = nullptr;

    } transaction_t;

    class frame_queue {
        public:
            frame_queue(std::shared_ptr<uvgrtp::socket> socket, std::shared_ptr<uvgrtp::rtp> rtp, int rce_flags);
            ~frame_queue();

            rtp_error_t init_transaction();
            rtp_error_t init_transaction(uint8_t *data);
            rtp_error_t init_transaction(std::unique_ptr<uint8_t[]> data);

            /* Releases all memory associated with transaction
             *
             * Return RTP_OK on success
             * Return RTP_INVALID_VALUE if "key" doesn't point to valid transaction */
            rtp_error_t deinit_transaction();

            /* Cache "message" to frame queue
             *
             * Return RTP_OK on success
             * Return RTP_INVALID_VALUE if one of the parameters is invalid
             * Return RTP_MEMORY_ERROR if the maximum amount of chunks/messages is exceeded */
            rtp_error_t enqueue_message(uint8_t *message, size_t message_len);
            rtp_error_t enqueue_message(uint8_t *message, size_t message_len, bool set_m_bit);

            /* Cache all messages in "buffers" in order to frame queue
             *
             * Return RTP_OK on success
             * Return RTP_INVALID_VALUE if one of the parameters is invalid
             * Return RTP_MEMORY_ERROR if the maximum amount of chunks/messages is exceeded */
            rtp_error_t enqueue_message(buf_vec& buffers);

            /* Flush the message queue
             *
             * Return RTP_OK on success
             * Return RTP_INVALID_VALUE if "sender" is nullptr or message buffer is empty
             * return RTP_SEND_ERROR if send fails */
            rtp_error_t flush_queue(sockaddr_in& addr, sockaddr_in6& addr6);

            /* Media may have extra headers (f.ex. NAL and FU headers for HEVC).
             * These headers must be valid until the message is sent (ie. they cannot be saved to
             * caller's stack).
             *
             * buf_vec is the place to store these extra headers (see src/formats/hevc.cc) */
            uvgrtp::buf_vec* get_buffer_vector();

            /* Each media may allocate extra buffers for the transaction struct if need be
             *
             *
             * Return pointer to media headers if they're set
             * Return nullptr if they're not set */
            void *get_media_headers();

            /* Update the active task's current packet's sequence number */
            void update_rtp_header();

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
             * to push_frame() must be deallocated manually. uvgRTP doesn't know the memory
             * type (or whether can be even deallocated) so application must provide a way
             * to deallocate the chunk
             *
             * If raw pointer without RTP_COPY is given to push_frame() and the deallocation
             * hook is missing, uvgRTP won't do anything about the memory which can lead to
             * significant memory leaks */
            void install_dealloc_hook(void (*dealloc_hook)(void *));

            void set_fps(ssize_t numerator, ssize_t denominator)
            {
                fps_ = numerator > 0 && denominator > 0;
                if (denominator > 0)
                {
                    frame_interval_ = std::chrono::nanoseconds(uint64_t(1.0 / double(numerator / denominator) * 1000*1000*1000));
                }
                frames_since_sync_ = 0; 
                force_sync_ = true;
            }

        private:

            void enqueue_finalize(uvgrtp::buf_vec& tmp);

            inline std::chrono::high_resolution_clock::time_point this_frame_time();

            inline void update_sync_point();

            transaction_t *active_;

            /* Deallocation hook is stored here and copied to transaction upon initialization */
            void (*dealloc_hook_)(void *);

            ssize_t max_mcount_; /* number of messages per transactions */
            ssize_t max_ccount_; /* number of chunks per message */

            std::shared_ptr<uvgrtp::rtp> rtp_;
            std::shared_ptr<uvgrtp::socket> socket_;

            int rce_flags_;

            bool fps_ = false;
            std::chrono::nanoseconds frame_interval_;

            std::chrono::high_resolution_clock::time_point fps_sync_point_;
            uint64_t frames_since_sync_ = 0;

            bool force_sync_ = false;
    };
}

namespace uvg_rtp = uvgrtp;
