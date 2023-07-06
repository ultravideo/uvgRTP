#include <uvgrtp/lib.hh>
#include <cstring>
#include <algorithm>
#include <string>
#include <iostream>
#include <vector>
#include <atomic>
#include <chrono>
#include <fstream>

struct thread_info {
    int pkts;
    size_t bytes;
    std::chrono::high_resolution_clock::time_point start;
    std::chrono::high_resolution_clock::time_point last;
} *thread_info;

 // Network parameters of this example
constexpr char SENDER_ADDRESS[] = "10.21.25.29";
constexpr uint16_t SENDER_PORT = 8888;
constexpr int TIMEOUT = 3000;

constexpr char RECEIVER_ADDRESS[] = "10.21.25.2";
constexpr uint16_t RECEIVER_PORT = 8888;

constexpr int THREADS = 5;
std::string RESULT_FILE = "results.txt";

void hook(void* arg, uvg_rtp::frame::rtp_frame* frame);
void receiver_func(uvgrtp::session* session, uint16_t sender_port, uint16_t receiver_port, int flags, int thread_num, int mux);

void write_receive_results_to_file(const std::string& filename,
    const size_t bytes, const size_t packets, const uint64_t diff_ms);

int main(int argc, char** argv)
{
    if (argc != 1) {
        std::cout << "Missing arguments" << std::endl;
        return EXIT_FAILURE;
    }
    int mux = atoi(argv[0]);

    int flags = RCE_NO_FLAGS;
    uvgrtp::context ctx;
    uvgrtp::session* session = ctx.create_session(SENDER_ADDRESS, RECEIVER_ADDRESS);
    uvgrtp::media_stream* conf_stream = nullptr;
    if (mux == 1) {
        conf_stream = session->create_stream(SENDER_PORT, RECEIVER_PORT, RTP_FORMAT_H264, flags, 99, 99);
        conf_stream->configure_ctx(RCC_UDP_RCV_BUF_SIZE, 40 * 1000 * 1000);
        conf_stream->configure_ctx(RCC_RING_BUFFER_SIZE, 40 * 1000 * 1000);
    }

    std::vector<std::thread*> threads = {};

    for (int i = 0; i < THREADS; ++i) {
        threads.push_back(new std::thread(receiver_func, session, SENDER_PORT, RECEIVER_PORT, flags, i, mux));
    }

    for (int i = 0; i < THREADS; ++i) {
        if (threads[i]->joinable())
        {
            threads[i]->join();
        }
        delete threads[i];
        threads[i] = nullptr;
    }
    threads.clear();
    if (mux == 1) {
        session->destroy_stream(conf_stream);
    }
    if (session) {
        ctx.destroy_session(session);
    }
    return EXIT_SUCCESS;
}

void receiver_func(uvgrtp::session* session, uint16_t sender_port, uint16_t receiver_port, int flags, int thread_num, int mux)
{
    uint32_t remote_ssrc = 10 + thread_num;
    uint16_t thread_recv_port = RECEIVER_PORT;
    uint16_t thread_send_port = SENDER_PORT;
    if (mux == 0) {
        thread_recv_port = receiver_port + thread_num * 2;
        thread_send_port = sender_port + thread_num * 2;
    }
    uvgrtp::media_stream* recv = session->create_stream(thread_send_port, thread_recv_port, RTP_FORMAT_H264, flags, remote_ssrc, 0);

    if (mux == 0) {
        recv->configure_ctx(RCC_UDP_RCV_BUF_SIZE, 40 * 1000 * 1000);
        recv->configure_ctx(RCC_RING_BUFFER_SIZE, 40 * 1000 * 1000);
        recv->configure_ctx(RCC_PKT_MAX_DELAY, 200);
    }
    if (!recv)
    {
        std::cout << "Error creating media stream" << std::endl;
    }
    std::cout << "Media stream created: " << thread_send_port << "->" << thread_recv_port << ", rem ssrc: " << remote_ssrc << std::endl;
    int tid = thread_num / 2;
    if (recv->install_receive_hook(&tid, hook)) {
        std::chrono::high_resolution_clock::time_point last_received = std::chrono::high_resolution_clock::now();

        while (true) {
            if (last_received + std::chrono::milliseconds(TIMEOUT) <= std::chrono::high_resolution_clock::now())
            {
                write_receive_results_to_file(RESULT_FILE,
                    thread_info[tid].bytes, thread_info[tid].pkts,
                    std::chrono::duration_cast<std::chrono::milliseconds>(
                        thread_info[tid].last - thread_info[tid].start).count());
                break;
            }

            // sleep so we don't busy loop
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        
    }
    else
    {
        std::cerr << "Failed to install receive hook. Aborting test" << std::endl;
    }
    session->destroy_stream(recv);
}


void hook(void* arg, uvgrtp::frame::rtp_frame* frame)
{
    int tid = *(int*)arg;

    if (thread_info[tid].pkts == 0) {
        thread_info[tid].start = std::chrono::high_resolution_clock::now();
    }

    thread_info[tid].last = std::chrono::high_resolution_clock::now();
    thread_info[tid].bytes += frame->payload_len;

    (void)uvg_rtp::frame::dealloc_frame(frame);
    ++thread_info[tid].pkts;
}


void write_receive_results_to_file(const std::string& filename,
    const size_t bytes, const size_t packets, const uint64_t diff_ms)
{
    std::cout << "Writing receive results into file: " << filename << std::endl;

    std::ofstream result_file;
    result_file.open(filename, std::ios::out | std::ios::app | std::ios::ate);
    result_file << bytes << " " << packets << " " << diff_ms << std::endl;
    result_file.close();
}
