#pragma once

#include "uvgrtp/util.hh"

#include <mutex>
#include <unordered_map>
#include <vector>
#include <functional>
#include <memory>
#include <thread>
#include <condition_variable>
#include <atomic>
#include <deque>
#include <map>

#ifdef _WIN32
#include <ws2ipdef.h>
#else
#include <netinet/ip.h>
#include <sys/socket.h>
#endif

namespace uvgrtp {

    namespace frame {
        struct rtp_frame;
    }

    class socket;

    typedef void (*recv_hook)(void* arg, uvgrtp::frame::rtp_frame* frame);
    typedef void (*user_hook)(void* arg, uint8_t* payload, uint32_t payload_size);

    struct receive_pkt_hook {
        void* arg = nullptr;
        recv_hook hook = nullptr;
    };

    typedef rtp_error_t (*packet_handler)(ssize_t, void *, int, uvgrtp::frame::rtp_frame **);
    typedef rtp_error_t (*packet_handler_aux)(void *, int, uvgrtp::frame::rtp_frame **);
    typedef rtp_error_t (*frame_getter)(void *, uvgrtp::frame::rtp_frame **);

    struct auxiliary_handler {
        void *arg = nullptr;
        packet_handler_aux handler = nullptr;
        frame_getter getter = nullptr;
    };

    struct auxiliary_handler_cpp {
        std::function<rtp_error_t(int, uvgrtp::frame::rtp_frame** out)> handler;
        std::function<rtp_error_t(uvgrtp::frame::rtp_frame** out)> getter;
    };

    struct packet_handlers {
        packet_handler primary = nullptr;
        std::vector<auxiliary_handler> auxiliary;
        std::vector<auxiliary_handler_cpp> auxiliary_cpp;
    };

    /* This class handles the reception processing of received RTP packets. It 
     * utilizes function dispatching to other classes to achieve this.

     * The point of reception flow is to provide isolation between different layers
     * of uvgRTP. For example, HEVC handler should not concern itself with RTP packet validation
     * because that should be a global operation done for all packets. Neither should Opus handler
     * take SRTP-provided authentication tag into account when it is performing operations on
     * the packet and ZRTP packets should not be relayed from media handler
     * to ZRTP handler et cetera.
     *
     * This can be achieved by having a global UDP packet handler for any packet type that validates
     * all common stuff it can and then dispatches the validated packet to the correct layer using
     * one of the installed handlers.
     *
     * If it's unclear as to which handler should be called, the packet is dispatched to all relevant
     * handlers and a handler then returns RTP_OK/RTP_PKT_NOT_HANDLED based on whether the packet was handled.
     *
     * For example, if receiver detects an incoming ZRTP packet, that packet is immediately dispatched to the
     * installed ZRTP handler if ZRTP has been enabled.
     * Likewise, if RTP packet authentication has been enabled, processor validates the packet before passing
     * it onto any other layer so all future work on the packet is not done in vain due to invalid data
     *
     * One piece of design choice that complicates the design of packet dispatcher a little is that the order
     * of handlers is important. First handler must be ZRTP and then follows SRTP, RTP and finally media handlers.
     * This requirement gives packet handler a clean and generic interface while giving a possibility to modify
     * the packet in each of the called handlers if needed. For example SRTP handler verifies RTP authentication
     * tag and decrypts the packet and RTP handler verifies the fields of the RTP packet and processes it into
     * a more easily modifiable format for the media handler.
     *
     * If packet is modified by the handler but the frame is not ready to be returned to user,
     * handler returns RTP_PKT_MODIFIED to indicate that it has modified the input buffer and that
     * the packet should be passed onto other handlers.
     *
     * When packet is ready to be returned to user, "out" parameter of packet handler is set to point to
     * the allocated frame that can be returned and return value of the packet handler is RTP_PKT_READY.
     *
     * If a handler receives a non-null "out", it can safely ignore "packet" and operate just on
     * the "out" parameter because at that point it already contains all needed information. */

    class reception_flow{
        public:
            reception_flow(bool ipv6);
            ~reception_flow();

            /* Install a primary handler for an incoming UDP datagram
             *
             * This handler is responsible for creating an operable RTP packet
             * that auxiliary handlers can work with.
             *
             * It is also responsible for validating the packet on a high level
             * (ZRTP checksum/RTP version etc) before passing it onto other handlers.
             *
             * Return a key on success that differentiates primary packet handlers
             * Return 0 "handler" is nullptr */
            uint32_t install_handler(packet_handler handler);

            /* Install auxiliary handler for the packet
             *
             * This handler is responsible for doing auxiliary operations on the packet
             * such as gathering sessions statistics data or decrypting the packet
             * It is called only after the primary handler of the auxiliary handler is called
             *
             * "key" is used to specify for which primary handlers for "handler"
             * An auxiliary handler can be installed to multiple primary handlers
             *
             * "arg" is an optional argument that is passed to the handler when it's called
             * It can be null if the handler does not require additional data
             *
             * Return RTP_OK on success
             * Return RTP_INVALID_VALUE if "handler" is nullptr or if "key" is not valid */
            rtp_error_t install_aux_handler(uint32_t key, void *arg, packet_handler_aux handler, frame_getter getter);

            rtp_error_t install_aux_handler_cpp(uint32_t key, 
                std::function<rtp_error_t(int, uvgrtp::frame::rtp_frame**)> handler,
                std::function<rtp_error_t(uvgrtp::frame::rtp_frame**)> getter);

            /* Install receive hook in reception flow
             *
             * Return RTP_OK on success
             * Return RTP_INVALID_VALUE if "hook" is nullptr */
            rtp_error_t install_receive_hook(void *arg, void (*hook)(void *, uvgrtp::frame::rtp_frame *), std::shared_ptr<std::atomic<std::uint32_t>> remote_ssrc);

            /* Start the RTP reception flow. Start querying for received packets and processing them.
             *
             * Return RTP_OK on success
             * Return RTP_MEMORY_ERROR if allocation of a thread object fails */
            rtp_error_t start(std::shared_ptr<uvgrtp::socket> socket, int rce_flags);

            /* Stop the RTP reception flow and wait until the receive loop is exited
             * to make sure that destroying the object is safe.
             *
             * Return RTP_OK on success */
            rtp_error_t stop();

            /* Fetch frame from the frame queue that contains all received frame.
             * pull_frame() will block until there is a frame that can be returned.
             * If "timeout" is given, pull_frame() will block only for however long
             * that value tells it to.
             * If no frame is received within that time period, pull_frame() returns nullptr
             * If remote SSRC is given, only pull frames that come from a source with this ssrc
             *
             * Return pointer to RTP frame on success
             * Return nullptr if operation timed out or an error occurred */
            uvgrtp::frame::rtp_frame *pull_frame();
            uvgrtp::frame::rtp_frame *pull_frame(ssize_t timeout_ms);
            uvgrtp::frame::rtp_frame* pull_frame(std::shared_ptr<std::atomic<std::uint32_t>> remote_ssrc);
            uvgrtp::frame::rtp_frame* pull_frame(ssize_t timeout_ms, std::shared_ptr<std::atomic<std::uint32_t>> remote_ssrc);

            /* Map a packet handler key to a REMOTE SSRC of a stream
             *
             * Return true if a handler with the given key exists in the reception_flow -> mapping succesful
             * Return false if there is no handler with this key -> no mapping is done */
            bool map_handler_key(uint32_t key, std::shared_ptr<std::atomic<std::uint32_t>> remote_ssrc);

            /* Clear the packet handlers associated with this handler key from the reception_flow
             * Also clear the hooks associated with this remote_ssrc
             * 
             * Return 1 if the hooks and handlers were cleared and there is no hooks or handlers left in
             * this reception_flow -> the flow can be safely deleted if wanted
             * Return 0 if the hooks and handlers were removed but there is still others left in this reception_flow */
            int clear_stream_from_flow(std::shared_ptr<std::atomic<std::uint32_t>> remote_ssrc, uint32_t handler_key);

            /// \cond DO_NOT_DOCUMENT
            void set_buffer_size(const ssize_t& value);
            ssize_t get_buffer_size() const;
            void set_payload_size(const size_t& value);
            rtp_error_t install_user_hook(void* arg, void (*hook)(void*, uint8_t* payload));
            /// \endcond

        private:
            /* RTP packet receiver thread */
            void receiver(std::shared_ptr<uvgrtp::socket> socket);

            /* RTP packet dispatcher thread */
            void process_packet(int rce_flags);

            /* Return a processed RTP frame to user either through frame queue or receive hook */
            void return_frame(uvgrtp::frame::rtp_frame *frame);

            void return_user_pkt(uint8_t* pkt);

            /* Call auxiliary handlers of a primary handler */
            void call_aux_handlers(uint32_t key, int rce_flags, uvgrtp::frame::rtp_frame **frame, uint8_t* ptr);

            inline void increase_buffer_size(ssize_t next_write_index);

            /* Primary handlers for the socket */
            std::unordered_map<uint32_t, packet_handlers> packet_handlers_;

            inline ssize_t next_buffer_location(ssize_t current_location);

            void create_ring_buffer();
            void destroy_ring_buffer();

            void clear_frames();

            /* If receive hook has not been installed, frames are pushed to "frames_"
             * and they can be retrieved using pull_frame() */
            std::deque<uvgrtp::frame::rtp_frame *> frames_;
            std::mutex frames_mtx_;

            //void *recv_hook_arg_;
            //void (*recv_hook_)(void *arg, uvgrtp::frame::rtp_frame *frame);

            std::map<std::shared_ptr<std::atomic<std::uint32_t>>, receive_pkt_hook> hooks_;
            // Map handler keys to media streams remote ssrcs
            std::map<uint32_t, std::shared_ptr<std::atomic<std::uint32_t>>> handler_mapping_;

            std::mutex flow_mutex_;
            bool should_stop_;

            std::unique_ptr<std::thread> receiver_;
            std::unique_ptr<std::thread> processor_;

            // from/from6 is the IP address that this packet came from
            struct Buffer
            {
                uint8_t* data;
                int read;
                sockaddr_in6 from6;
                sockaddr_in from;
            };

            void* user_hook_arg_;
            void (*user_hook_)(void* arg, uint8_t* payload);

            std::vector<Buffer> ring_buffer_;
            std::mutex ring_mutex_;
            // these uphold the ring buffer details
            std::atomic<ssize_t> ring_read_index_;
            std::atomic<ssize_t> last_ring_write_index_;

            std::mutex wait_mtx_; // for waking up the processing thread (read)

            std::condition_variable process_cond_;
            std::shared_ptr<uvgrtp::socket> socket_;

            ssize_t buffer_size_kbytes_;
            size_t payload_size_;
            bool active_;
            bool ipv6_;
    };
}

namespace uvg_rtp = uvgrtp;
