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
    class rtcp;

    typedef void (*recv_hook)(void* arg, uvgrtp::frame::rtp_frame* frame);

    typedef void (*user_hook)(void* arg, uint8_t* data, uint32_t len);

    struct receive_pkt_hook {
        void* arg = nullptr;
        recv_hook hook = nullptr;
    };

    typedef rtp_error_t (*frame_getter)(void *, uvgrtp::frame::rtp_frame **);

    struct packet_handler {
        std::function<rtp_error_t(void*, int, uint8_t*, size_t, frame::rtp_frame** out)> handler;
        void* args = nullptr;
    };
    struct handler {
        packet_handler rtp;
        packet_handler rtcp;
        packet_handler zrtp;
        packet_handler srtp;
        packet_handler media;
        packet_handler rtcp_common;
        std::function<rtp_error_t(uvgrtp::frame::rtp_frame ** out)> getter;
    };

    /* This class handles the reception processing of received RTP packets. It 
     * utilizes function dispatching to other classes to achieve this.
     *
     * Each socket has a reception flow for receiving and handling packets from the socket.
     * Media streams then install packet handlers into reception flow. When installing
     * a handler, a *REMOTE SSRC* is given. This SSRC is the source that this media stream
     * wants to receive packets *from*.
     
     * When processing packets, reception flow looks at the source SSRC in the packet header
     * and sends it to the handlers that want to receive from this remote source.
     * Various checks are done on the packet, and the packet is determined to be either a 
     * 1. RTCP packet (if RCE_RTCP_MUX is enabled, otherwise RTCP uses its own socket)
     * 2. ZRTP packet
     * 3. SRTP packet
     * 4. RTP packet
     * 5. Holepuncher packet
     * 
     * The packet is then sent to the correct handler of the correct media stream.
     * When multiplexing several media streams into a single socket, SSRC is what 
     * separates one stream from another. You can also give each media stream pair
     * their own ports, which eliminates the need for SSRC checking. In this case
     * each reception_flow object will have just a single set of packet handlers
     * and all packets are given to these.
     * 
     * ---- NOTE: User packets disabled for now ----
     * If there is no valid SSRC to be found in the received packet's header, the
     * packet is assumed to be a user packet, in which case it is handed over to 
     * a user packet handler, provided that there is one installed. */

    class reception_flow{
        public:
            reception_flow(bool ipv6);
            ~reception_flow();

            /* Install a new packet handler into the reception flow.
            *  Types: Each handler type corresponds to an integerm, as follows:
               1 RTP 
               2 RTCP
               3 ZRTP
               4 SRTP
               5 Media
               6 RTCP common: Updates RTCP stats from RTP packets */
            rtp_error_t install_handler(int type, std::shared_ptr<std::atomic<std::uint32_t>> remote_ssrc, 
                std::function<rtp_error_t(void*, int, uint8_t*, size_t, frame::rtp_frame** out)> handler,
                void* args);

            /* Install a media getter. If multiple packets are ready, this is called. */
            rtp_error_t install_getter(std::shared_ptr<std::atomic<std::uint32_t>> remote_ssrc,
                std::function<rtp_error_t(uvgrtp::frame::rtp_frame**)> getter);

            /* Remove all handlers associated with this SSRC */
            rtp_error_t remove_handlers(std::shared_ptr<std::atomic<std::uint32_t>> remote_ssrc);

            /* Install receive hook in reception flow
             * Return RTP_OK on success
             * Return RTP_INVALID_VALUE if "hook" is nullptr */
            rtp_error_t install_receive_hook(void *arg, void (*hook)(void *, uvgrtp::frame::rtp_frame *), uint32_t remote_ssrc);

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

            /* Clear the packet handlers associated with this REMOTE SSRC
             * Also clear the hooks associated with this remote_ssrc
             * 
             * Return 1 if the hooks and handlers were cleared and there is no hooks or handlers left in
             * this reception_flow -> the flow can be safely deleted if wanted
             * Return 0 if the hooks and handlers were removed but there is still others left in this reception_flow */
            int clear_stream_from_flow(std::shared_ptr<std::atomic<std::uint32_t>> remote_ssrc);

            /* Update the remote SSRC linked to packet handlers and media hooks
             * Used with media_stream.configure_ctx(RCC_REMOTE_SSRC, ---);
             * Return RTP_OK on success */
            rtp_error_t update_remote_ssrc(uint32_t old_remote_ssrc, uint32_t new_remote_ssrc);

            /// \cond DO_NOT_DOCUMENT
            void set_buffer_size(const ssize_t& value);
            ssize_t get_buffer_size() const;
            void set_payload_size(const size_t& value);
            void set_poll_timeout_ms(int timeout_ms);
            int get_poll_timeout_ms();

            // DISABLED rtp_error_t install_user_hook(void* arg, void (*hook)(void*, uint8_t* data, uint32_t len));
            /// \endcond

        private:
            /* RTP packet receiver thread */
            void receiver(std::shared_ptr<uvgrtp::socket> socket);

            /* RTP packet dispatcher thread */
            void process_packet(int rce_flags);

            /* Return a processed RTP frame to user either through frame queue or receive hook */
            void return_frame(uvgrtp::frame::rtp_frame *frame);

            //void return_user_pkt(uint8_t* pkt, uint32_t len);

            inline void increase_buffer_size(ssize_t next_write_index);

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

            std::unordered_map<uint32_t, receive_pkt_hook> hooks_;

            std::mutex flow_mutex_;
            bool should_stop_;

            std::unique_ptr<std::thread> receiver_;
            std::unique_ptr<std::thread> processor_;

            // from/from6 is the IP address that this packet came from
            struct Buffer
            {
                uint8_t* data;
                int read;
                //sockaddr_in6 from6;
                //sockaddr_in from;
            };

            void* user_hook_arg_;
            void (*user_hook_)(void* arg, uint8_t* data, uint32_t len);

            // Map different types of handlers by remote SSRC
            std::unordered_map<uint32_t, handler> packet_handlers_;

            int poll_timeout_ms_;

            std::vector<Buffer> ring_buffer_;
            std::mutex handlers_mutex_;
            std::mutex ring_mutex_;
            std::mutex active_mutex_;
            std::mutex hooks_mutex_;

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
