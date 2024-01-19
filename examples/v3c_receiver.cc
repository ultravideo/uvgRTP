#include <uvgrtp/lib.hh>
#include "v3c/v3c_util.hh"

#include <thread>
#include <iostream>
#include <fstream>
#include <chrono>

/* This example demonstrates receiving a V3C Sample Stream via uvgRTP. It can be used to send V-PCC encoded files, but with
 * minor modifications (addition of V3C_CAD and V3C_PVD streams) it can be used also for MIV encoded files. See the comments
 * in v3c_sender for more comprehensive documentation on V3C RTP streaming.
 
 * Video data can be either AVC, HEVC or VVC encoded. This example uses HEVC encoding. Using AVC or VVC requires you to
 * set the media streams payload format accordingly.
 */

constexpr char LOCAL_IP[] = "127.0.0.1";

// This example runs for 10 seconds
constexpr auto RECEIVE_TIME_S = std::chrono::seconds(10);

// Hooks for the media streams
void vps_receive_hook(void* arg, uvgrtp::frame::rtp_frame* frame); // VPS only included for simplicity of demonstration
void ad_receive_hook(void* arg, uvgrtp::frame::rtp_frame* frame);
void ovd_receive_hook(void* arg, uvgrtp::frame::rtp_frame* frame);
void gvd_receive_hook(void* arg, uvgrtp::frame::rtp_frame* frame);
void avd_receive_hook(void* arg, uvgrtp::frame::rtp_frame* frame);

uint64_t vps_count;

/* These values specify the amount of NAL units inside each type of V3C unit. These need to be known to be able to reconstruct the 
 * GOFs after receiving. These default values correspond to the provided test sequence, and may be different for other sequences.
 * The sending example has prints that show how many NAL units each V3C unit contain. For other sequences, these values can be
 * modified accordingly. */
constexpr int VPS_NALS = 2; // VPS only included for simplicity of demonstration
constexpr int AD_NALS = 35;
constexpr int OVD_NALS = 35;
constexpr int GVD_NALS = 131;
constexpr int AVD_NALS = 131;

/* NOTE: In case where the last GOF has fewer NAL units than specified above, the receiver does not know how many to expect
   and cannot reconstruct that specific GOF. s*/

/* How many *FULL* Groups of Frames we are expecting to receive */
constexpr int EXPECTED_GOFS = 9;

/* Path to the V3C file that we are receiving. This is included so that you can check that the reconstructed full GOFs are equal to the
 * original ones */
std::string PATH = "";

bool write_file(const char* data, size_t len, const std::string& filename);

int main(void)
{
    std::cout << "Starting uvgRTP V3C receive hook example" << std::endl;

    /* Initialize uvgRTP context and session*/
    uvgrtp::context ctx;
    std::pair<std::string, std::string> addresses_receiver(LOCAL_IP, LOCAL_IP);
    uvgrtp::session* sess = ctx.create_session(addresses_receiver);
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

    uint64_t ngofs = 0;     // Number of received GOFs
    uint64_t bytes = 0;     // Number of received bytes
    uint64_t ptr = 0;       // Pointer of current position on the received file
    bool hdb = true;        // Write header byte or not. True only for first GOF of file.

    // Save each GOF into data structures
    struct gof_info {       
        uint64_t size = 0;
        char* buf = nullptr;
    };
    std::map<uint32_t, gof_info> gofs_buf = {};

    std::chrono::high_resolution_clock::time_point start = std::chrono::high_resolution_clock::now();;

    while (ngofs < EXPECTED_GOFS) {
        // Check if we received enough NAL units of a GOF
        if (is_gof_ready(ngofs, mmap)) {

            // Get GOF size and initialize a new data structure for it
            uint64_t gof_len = get_gof_size(hdb, ngofs, mmap);
            gof_info cur = {gof_len, new char[gof_len]};
            gofs_buf.insert({ ngofs, cur });

            ptr = 0; //Don't reset the ptr because we are writing the whole file into the buffer

            // Reconstruct the GOF from NAL units and update size of the to-be complete file. NOTE: Not the same as the amount of
            // received bytes, because we add new info such as V3C unit headers here.
            bytes +=  reconstruct_v3c_gof(hdb, gofs_buf.at(ngofs).buf, ptr, mmap, ngofs);
            std::cout << "Full GOF received, num: " << ngofs << std::endl;
            ngofs++;
            hdb = false; // Only add the V3C Sample Stream header byte to only the first GOF
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        
        auto runtime = (uint64_t)std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::high_resolution_clock::now() - start).count();

        if (runtime > RECEIVE_TIME_S.count()*1000) {
            std::cout << "Timeout" << std::endl;
            break;
        }
    }
    std::cout << ngofs << " full GOFs received" << std::endl;

    /* Reception done, clean up uvgRTP */
    sess->destroy_stream(streams.vps);
    sess->destroy_stream(streams.ad);
    sess->destroy_stream(streams.ovd);
    sess->destroy_stream(streams.gvd);
    sess->destroy_stream(streams.avd);

    ctx.destroy_session(sess);

    // Not we have all the GOFs constructed. Next up save them all into a single file
    char* out_buf = new char[bytes];
    std::memset(out_buf, 0, bytes); // Initialize with zeros

    uint64_t ptr2 = 0;
    // Reconstruct file from GOFs
    for (auto& p : gofs_buf) {
        memcpy(&out_buf[ptr2], p.second.buf , p.second.size);
        ptr2 += p.second.size;
    }

    /* Read the original file and its size for verification */
    uint64_t len = get_size(PATH);
    if (len == 0) {
        return EXIT_FAILURE;
    }
    std::cout << "Reading original file for comparison " << len << std::endl;
    char* original_buf = nullptr;
    original_buf = get_cmem(PATH);

    bool diff = false;
    // Compare reconstructed file with the original one
    for (auto i = 0; i < bytes; ++i) {
        if (original_buf[i] != out_buf[i]) {
            std::cout << "Difference at " << i << std::endl;
            diff = true;
            break;
        }
    }
    if (!diff) {
        std::cout << "No difference found in " << EXPECTED_GOFS << " GOFs" << std::endl;
    }

    delete[] out_buf;

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
