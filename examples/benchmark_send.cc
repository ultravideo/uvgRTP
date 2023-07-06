#include <uvgrtp/lib.hh>
#include <climits>
#include <cstring>
#include <vector>
#include <iostream>
#include <string>
#include <fstream>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <cstdio>
#include <cstdlib>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <unistd.h>

/* Linux only */

 // Network parameters of this example
constexpr char SENDER_ADDRESS[] = "127.0.0.1";
constexpr uint16_t SENDER_PORT = 8888;
constexpr char RECEIVER_ADDRESS[] = "127.0.0.1";
constexpr uint16_t RECEIVER_PORT = 8888;

std::string FILENAME = "test.hevc";
std::string RESULT_FILE = "results.txt";

// demonstration parameters of this example
constexpr int FPS = 30;
constexpr int THREADS = 5;

void sender_func(uvgrtp::session* session, uint16_t sender_port, uint16_t receiver_port, int flags, int fps, int thread_num,
    std::vector<uint64_t> chunk_sizes, void* mem, int mux);
void* get_mem(std::string filename, size_t& len);
void get_chunk_sizes(std::string filename, std::vector<uint64_t>& chunk_sizes);
std::string get_chunk_filename(std::string& encoded_filename);

void write_send_results_to_file(const std::string& filename,
    const size_t bytes, const uint64_t diff);

int main(int argc, char **argv)
{
    if (argc != 1) {
        std::cout << "Missing arguments" << std::endl;
        return EXIT_FAILURE;
    }
    int mux = atoi(argv[0]);

    int flags = RCE_NO_FLAGS;

    size_t len = 0;
    void* mem = get_mem(FILENAME, len);

    std::vector<uint64_t> chunk_sizes;
    get_chunk_sizes(get_chunk_filename(FILENAME), chunk_sizes);

    if (mem == nullptr || chunk_sizes.empty())
    {
        std::cerr << "Failed to get file: " << FILENAME << std::endl;
        std::cerr << "or chunk location file: " << get_chunk_filename(FILENAME) << std::endl;
        return EXIT_FAILURE;
    }

    uvgrtp::context ctx;
    uvgrtp::session* session = ctx.create_session(RECEIVER_ADDRESS, SENDER_ADDRESS);
    uvgrtp::media_stream* conf_stream = nullptr;
    if (mux == 1) {
        conf_stream = session->create_stream(RECEIVER_PORT, SENDER_PORT, RTP_FORMAT_H264, flags, 99, 99);
        conf_stream->configure_ctx(RCC_UDP_SND_BUF_SIZE, 40 * 1000 * 1000);
    }

    std::vector<std::thread*> threads = {};

    for (int i = 0; i < THREADS; ++i) {
        threads.push_back(new std::thread(sender_func, session, SENDER_PORT, RECEIVER_PORT, flags, FPS, i, mux));
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

void sender_func(uvgrtp::session* session, uint16_t sender_port, uint16_t receiver_port, int flags, int fps, int thread_num,
    std::vector<uint64_t> chunk_sizes, void* mem, int mux)
{
    uvgrtp::media_stream* send = nullptr;
    uint32_t local_ssrc = 10 + thread_num;
    uint16_t thread_recv_port = RECEIVER_PORT;
    uint16_t thread_send_port = SENDER_PORT;
    if (mux == 0) {
        thread_recv_port = receiver_port + thread_num * 2;
        thread_send_port = sender_port + thread_num * 2;
    }
    uvgrtp::media_stream* send = session->create_stream(receiver_port, sender_port, RTP_FORMAT_H264, flags, 0, local_ssrc);

    send->configure_ctx(RCC_FPS_NUMERATOR, fps);

    if (mux == 0) {
        send->configure_ctx(RCC_UDP_SND_BUF_SIZE, 40 * 1000 * 1000);
    }

    if (!send)
    {
        std::cout << "Error creating media stream" << std::endl;
    }
    rtp_error_t ret = RTP_OK;
    size_t bytes_sent = 0;
    uint64_t current_frame = 0;
    uint64_t period = (uint64_t)((1000 / (double)fps) * 1000);

    std::chrono::high_resolution_clock::time_point start = std::chrono::high_resolution_clock::now();

    for (auto& chunk_size : chunk_sizes)
    {
        if ((ret = send->push_frame((uint8_t*)mem + bytes_sent, chunk_size, 0)) != RTP_OK) {

            fprintf(stderr, "push_frame() failed!\n");

            // there is probably something wrong with the benchmark setup if push_frame fails
            std::cerr << "Send test push failed! Please fix benchmark suite." << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            session->destroy_stream(send);
            return;
        }

        bytes_sent += chunk_size;
        current_frame += 1;

        auto runtime = (uint64_t)std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::high_resolution_clock::now() - start
        ).count();

        // this enforces the fps restriction by waiting until it is time to send next frame
        // if this was eliminated, the test would be just about sending as fast as possible.
        // if the library falls behind, it is allowed to catch up if it can do it.
        if (runtime < current_frame * period)
            std::this_thread::sleep_for(std::chrono::microseconds(current_frame * period - runtime));
    }

    auto end = std::chrono::high_resolution_clock::now();
    uint64_t diff = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    write_send_results_to_file(RESULT_FILE, bytes_sent, diff);
    session->destroy_stream(send);

}

void write_send_results_to_file(const std::string& filename,
    const size_t bytes, const uint64_t diff)
{
    std::cout << "Writing send results into file: " << filename << std::endl;

    std::ofstream result_file;
    result_file.open(filename, std::ios::out | std::ios::app | std::ios::ate);
    result_file << bytes << " bytes, " << bytes / 1000 << " kB, " << bytes / 1000000 << " MB took "
        << diff << " ms " << diff / 1000 << " s" << std::endl;
    result_file.close();
}


void get_chunk_sizes(std::string filename, std::vector<uint64_t>& chunk_sizes)
{
    std::ifstream inputFile(filename, std::ios::in | std::ios::binary);

    if (!inputFile.good())
    {
        if (inputFile.eof())
        {
            std::cerr << "Input eof before starting" << std::endl;
        }
        else if (inputFile.bad())
        {
            std::cerr << "Input bad before starting" << std::endl;
        }
        else if (inputFile.fail())
        {
            std::cerr << "Input fail before starting" << std::endl;
        }

        return;
    }

    while (!inputFile.eof())
    {
        uint64_t chunk_size = 0;
        if (!inputFile.read((char*)&chunk_size, sizeof(uint64_t)))
        {
            break;
        }
        else
        {
            chunk_sizes.push_back(chunk_size);
        }
    }

    inputFile.close();
}

std::string get_chunk_filename(std::string& encoded_filename)
{
    std::string mem_file = "";
    std::string ending = "";
    // remove any possible file extensions and add hevc
    size_t lastindex = encoded_filename.find_last_of(".");

    if (lastindex != std::string::npos)
    {
        ending = encoded_filename.substr(lastindex + 1);
        mem_file = encoded_filename.substr(0, lastindex);
    }

    return mem_file + ".m" + ending;
}

void* get_mem(std::string filename, size_t& len)
{
    if (access(filename.c_str(), F_OK) == -1) {
        std::cerr << "Failed to access test file: " << filename << std::endl;
        return nullptr;
    }

    int fd = open(filename.c_str(), O_RDONLY, 0);

    if (fd < 0)
    {
        std::cerr << "Failed to open test file: " << filename << std::endl;
        return nullptr;
    }

    struct stat st;
    stat(filename.c_str(), &st);
    len = st.st_size;

    void* mem = mmap(NULL, len, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_POPULATE, fd, 0);

    if (mem)
    {
        madvise(mem, len, MADV_SEQUENTIAL | MADV_WILLNEED);
    }

    return mem;
}