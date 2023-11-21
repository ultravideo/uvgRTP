#include <uvgrtp/lib.hh>
#include "v3c/v3c_util.hh"

#include <thread>
#include <iostream>
#include <fstream>

constexpr char LOCAL_ADDRESS[] = "127.0.0.1";

// This example runs for 5 seconds
constexpr auto RECEIVE_TIME_S = std::chrono::seconds(5);

void vps_receive_hook(void* arg, uvgrtp::frame::rtp_frame* frame);
void ad_receive_hook(void* arg, uvgrtp::frame::rtp_frame* frame);
void ovd_receive_hook(void* arg, uvgrtp::frame::rtp_frame* frame);
void gvd_receive_hook(void* arg, uvgrtp::frame::rtp_frame* frame);
void avd_receive_hook(void* arg, uvgrtp::frame::rtp_frame* frame);

uint64_t vps_count;

/* These values specify the amount of NAL units inside each type of V3C unit. These need to be known to be able to reconstruct the 
 * file after receiving. These might be different for you depending on your test file. The sending example has prints that show
 * how many NAL units each V3C unit contain. Change these accordingly. */
constexpr int VPS_NALS = 2;
constexpr int AD_NALS = 35;
constexpr int OVD_NALS = 35;
constexpr int GVD_NALS = 131;
constexpr int AVD_NALS = 131;

/* How many Groups of Frames we are expecting to receive */
constexpr int EXPECTED_GOFS = 10;

/* Path to the V3C file that we are receiving.This is included so that you can check that the reconstructed file is equal to the
 * original one */
std::string PATH = "";
std::string RESULT_FILENAME = "received.vpcc";

bool write_file(const char* data, size_t len, const std::string& filename);

int main(void)
{
    std::cout << "Starting uvgRTP V3C receive hook example" << std::endl;

    /* Fetch the original file and its size */
    uint64_t len = get_size(PATH);
    std::cout << "File size " << len << std::endl;

    char* original_buf = nullptr;
    original_buf = get_cmem(PATH);

    uvgrtp::context ctx;
    uvgrtp::session* sess = ctx.create_session(LOCAL_ADDRESS, LOCAL_ADDRESS);
    int flags = 0;

    // Create the uvgRTP media streams with the correct RTP format
    v3c_streams streams = init_v3c_streams(sess, 8890, 8892, flags, true);

    // Initialize memory map
    v3c_file_map mmap = init_mmap();

    streams.vps->install_receive_hook(&mmap.vps_units, vps_receive_hook);
    streams.ad->install_receive_hook(&mmap.ad_units, ad_receive_hook);
    streams.ovd->install_receive_hook(&mmap.ovd_units, ovd_receive_hook);
    streams.gvd->install_receive_hook(&mmap.gvd_units, gvd_receive_hook);
    streams.avd->install_receive_hook(&mmap.avd_units, avd_receive_hook);
    streams.avd->configure_ctx(RCC_RING_BUFFER_SIZE, 40 * 1000 * 1000);

    std::cout << "Waiting incoming packets for " << RECEIVE_TIME_S.count() << " s" << std::endl;
    uint64_t ngofs = 0;
    uint64_t bytes = 0;
    uint64_t ptr = 0;
    bool hdb = true;
    struct gof_info {
        uint64_t size = 0;
        char* buf = nullptr;
    };
    std::map<uint32_t, gof_info> gofs_buf = {};

    while (ngofs < EXPECTED_GOFS) {
        if (is_gof_ready(ngofs, mmap)) {
            uint64_t gof_len = get_gof_size(hdb, ngofs, mmap);
            gof_info cur = {gof_len, new char[gof_len]};
            gofs_buf.insert({ ngofs, cur });
            ptr = 0; //Don't reset the ptr because we are writing the whole file into the buffer
            bytes +=  reconstruct_v3c_gof(hdb, gofs_buf.at(ngofs).buf, ptr, mmap, ngofs);
            std::cout << "Full GOF received, num: " << ngofs << std::endl;
            ngofs++;
            hdb = false; // Only add the V3C Sample Stream header byte to only the first GOF
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    std::cout << "output file size " << bytes << std::endl;
    //std::this_thread::sleep_for(RECEIVE_TIME_S); // lets this example run for some time

    sess->destroy_stream(streams.vps);
    sess->destroy_stream(streams.ad);
    sess->destroy_stream(streams.ovd);
    sess->destroy_stream(streams.gvd);
    sess->destroy_stream(streams.avd);

    ctx.destroy_session(sess);

    char* out_buf = new char[bytes];
    uint64_t ptr2 = 0;
    // reconstruct file from gofs
    for (auto& p : gofs_buf) {
        memcpy(&out_buf[ptr2], p.second.buf , p.second.size);
        ptr2 += p.second.size;
    }

    // compare files
    
    for (auto i = 0; i < bytes; ++i) {
        if (original_buf[i] != out_buf[i]) {
            std::cout << "Difference at " << i << std::endl;
            //break;
        }
    }
    /*
    std::cout << "Writing to file " << RESULT_FILENAME << std::endl;
    write_file(out_buf, bytes, RESULT_FILENAME);
    */
    std::cout << "Done" << std::endl;

    return EXIT_SUCCESS;
}

void vps_receive_hook(void* arg, uvgrtp::frame::rtp_frame* frame)
{
    std::vector<v3c_unit_info>* vec = (std::vector<v3c_unit_info>*)arg;

    char* cbuf = new char[frame->payload_len];
    memcpy(cbuf, frame->payload, frame->payload_len);
    v3c_unit_info vps = { {}, {{0, frame->payload_len, cbuf}} };
    vec->push_back(vps);
    
    (void)uvgrtp::frame::dealloc_frame(frame);
}
void ad_receive_hook(void* arg, uvgrtp::frame::rtp_frame* frame)
{
    std::vector<v3c_unit_info>* vec = (std::vector<v3c_unit_info>*)arg;
    copy_rtp_payload(vec, AD_NALS, frame);
    (void)uvgrtp::frame::dealloc_frame(frame);
}
void ovd_receive_hook(void* arg, uvgrtp::frame::rtp_frame* frame)
{
    std::vector<v3c_unit_info>* vec = (std::vector<v3c_unit_info>*)arg;
    copy_rtp_payload(vec, OVD_NALS, frame);
    (void)uvgrtp::frame::dealloc_frame(frame);
}
void gvd_receive_hook(void* arg, uvgrtp::frame::rtp_frame* frame)
{
    std::vector<v3c_unit_info>* vec = (std::vector<v3c_unit_info>*)arg;
    copy_rtp_payload(vec, GVD_NALS, frame);
    (void)uvgrtp::frame::dealloc_frame(frame);
}
void avd_receive_hook(void* arg, uvgrtp::frame::rtp_frame* frame)
{
    std::vector<v3c_unit_info>* vec = (std::vector<v3c_unit_info>*)arg;
    copy_rtp_payload(vec, AVD_NALS, frame);
    (void)uvgrtp::frame::dealloc_frame(frame);
}

bool write_file(const char* data, size_t len, const std::string& filename) {
    std::ofstream file(filename, std::ios::binary);

    if (!file.is_open()) {
        return false; // Failed to open the file
    }

    file.write(data, len);

    file.close();
    return true; // File write successful
}
