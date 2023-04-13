#pragma once

#include "uvgrtp/util.hh"
#include "reception_flow.hh"
#include <string>
#include <memory>
#include <vector>
#include <functional>


namespace uvgrtp {

    namespace frame {
        struct rtp_frame;
    }

    class socket;


    class socketfactory {

        public:
            socketfactory(int rce_flags);
            ~socketfactory();
            rtp_error_t stop();

            rtp_error_t set_local_interface(std::string local_addr);
            std::shared_ptr<uvgrtp::socket> create_new_socket();
            rtp_error_t bind_socket(std::shared_ptr<uvgrtp::socket> soc, uint16_t port);
            rtp_error_t bind_socket_anyip(std::shared_ptr<uvgrtp::socket> soc, uint16_t port);

            uint32_t install_handler(packet_handler handler);
            rtp_error_t install_aux_handler(uint32_t key, void* arg, packet_handler_aux handler, frame_getter getter);
            rtp_error_t install_aux_handler_cpp(uint32_t key,
                std::function<rtp_error_t(int, uvgrtp::frame::rtp_frame**)> handler,
                std::function<rtp_error_t(uvgrtp::frame::rtp_frame**)> getter);
            rtp_error_t install_receive_hook(void* arg, void (*hook)(void*, uvgrtp::frame::rtp_frame*));

            rtp_error_t start(std::shared_ptr<uvgrtp::socket> socket, int rce_flags);


            std::shared_ptr<uvgrtp::socket> get_socket_ptr() const;
            bool get_ipv6() const;

        private:
            void rcvr(std::shared_ptr<uvgrtp::socket> socket);
            inline ssize_t next_buffer_location(ssize_t current_location);
            void create_ring_buffer();
            void destroy_ring_buffer();
            void clear_frames();

            std::deque<uvgrtp::frame::rtp_frame*> frames_;
            std::mutex frames_mtx_;

            int rce_flags_;
            std::string local_address_;
            std::vector<uint16_t> used_ports_;
            bool ipv6_;
            std::vector<std::shared_ptr<uvgrtp::socket>> used_sockets_;

            void* recv_hook_arg_;
            void (*recv_hook_)(void* arg, uvgrtp::frame::rtp_frame* frame);
            std::unordered_map<uint32_t, packet_handlers> packet_handlers_;
            bool should_stop_;
            std::unique_ptr<std::thread> receiver_;
            //std::unique_ptr<std::thread> processor_;

            struct Buffer
            {
                uint8_t* data;
                int read;
            };

            std::vector<Buffer> ring_buffer_;
            std::atomic<ssize_t> ring_read_index_;
            std::atomic<ssize_t> last_ring_write_index_;
            std::condition_variable process_cond_;
            ssize_t buffer_size_kbytes_;
            size_t payload_size_;


    };
}