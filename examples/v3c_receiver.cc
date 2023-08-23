#include <uvgrtp/lib.hh>

#include <thread>
#include <iostream>
#include <fstream>

constexpr char LOCAL_ADDRESS[] = "127.0.0.1";

// This example runs for 5 seconds
constexpr auto RECEIVE_TIME_S = std::chrono::seconds(10);

void vps_receive_hook(void* arg, uvgrtp::frame::rtp_frame* frame);
void ad_receive_hook(void* arg, uvgrtp::frame::rtp_frame* frame);
void ovd_receive_hook(void* arg, uvgrtp::frame::rtp_frame* frame);
void gvd_receive_hook(void* arg, uvgrtp::frame::rtp_frame* frame);
void avd_receive_hook(void* arg, uvgrtp::frame::rtp_frame* frame);

uint64_t vps_count;

constexpr int VPS_NALS = 1;
constexpr int AD_NALS = 35;
constexpr int OVD_NALS = 35;
constexpr int GVD_NALS = 131;
constexpr int AVD_NALS = 131;
constexpr int EXPECTED_GOPS = 1;

std::string PATH = "C:\\Users\\ngheta\\Documents\\v3c_test_seq_2.vpcc";

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
    int flags = RCE_RECEIVE_ONLY;

    // Create the uvgRTP media streams with the correct RTP format
    v3c_streams streams = init_v3c_streams(sess, 8890, 8892, flags, true);
    //avd->configure_ctx(RCC_RING_BUFFER_SIZE, 40*1000*1000);
    v3c_file_map mmap = init_mmap();

    streams.vps->install_receive_hook(&mmap.vps_units, vps_receive_hook);
    streams.ad->install_receive_hook(&mmap.ad_units, ad_receive_hook);
    streams.ovd->install_receive_hook(&mmap.ovd_units, ovd_receive_hook);
    streams.gvd->install_receive_hook(&mmap.gvd_units, gvd_receive_hook);
    streams.avd->install_receive_hook(&mmap.avd_units, avd_receive_hook);


    std::cout << "Waiting incoming packets for " << RECEIVE_TIME_S.count() << " s" << std::endl;
    uint64_t ngops = 1;
    uint64_t bytes = 0;
    uint64_t ptr = 0;
    bool hdb = true;
    char* out_buf = nullptr;

    while (ngops <= EXPECTED_GOPS) {
        if (is_gop_ready(ngops, mmap)) {
            bytes +=  reconstruct_v3c_gop(hdb, out_buf, ptr, mmap, ngops - 1);
            std::cout << "Full GoP received, num: " << ngops << std::endl;
            ngops++;
            hdb = false; // Only add the V3C Sample Stream header byte to only the first GoP
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    std::this_thread::sleep_for(RECEIVE_TIME_S); // lets this example run for some time

    sess->destroy_stream(streams.vps);
    sess->destroy_stream(streams.ad);
    sess->destroy_stream(streams.ovd);
    sess->destroy_stream(streams.gvd);
    sess->destroy_stream(streams.avd);

    ctx.destroy_session(sess);

    // compare files
    for (auto i = 0; i < len; ++i) {
        if (original_buf[i] != out_buf[i]) {
            std::cout << "Difference at " << i << std::endl;
            break;
        }
    }
    std::cout << "Done " << std::endl;

    return EXIT_SUCCESS;
}

void vps_receive_hook(void* arg, uvgrtp::frame::rtp_frame* frame)
{
    std::vector<v3c_unit_info>* vec = (std::vector<v3c_unit_info>*)arg;

    char* cbuf = new char[frame->payload_len];
    memcpy(cbuf, frame->payload, frame->payload_len);
    v3c_unit_info vps = { {}, {{0, frame->payload_len}}, cbuf };
    vec->push_back(vps);
    
    (void)uvgrtp::frame::dealloc_frame(frame);
}
void ad_receive_hook(void* arg, uvgrtp::frame::rtp_frame* frame)
{
    std::vector<v3c_unit_info>* vec = (std::vector<v3c_unit_info>*)arg;
    copy_rtp_payload(*vec, AD_NALS, frame);
    (void)uvgrtp::frame::dealloc_frame(frame);
}
void ovd_receive_hook(void* arg, uvgrtp::frame::rtp_frame* frame)
{
    std::vector<v3c_unit_info>* vec = (std::vector<v3c_unit_info>*)arg;
    copy_rtp_payload(*vec, OVD_NALS, frame);
    (void)uvgrtp::frame::dealloc_frame(frame);
}
void gvd_receive_hook(void* arg, uvgrtp::frame::rtp_frame* frame)
{
    std::vector<v3c_unit_info>* vec = (std::vector<v3c_unit_info>*)arg;
    copy_rtp_payload(*vec, GVD_NALS, frame);
    (void)uvgrtp::frame::dealloc_frame(frame);
}
void avd_receive_hook(void* arg, uvgrtp::frame::rtp_frame* frame)
{
    std::vector<v3c_unit_info>* vec = (std::vector<v3c_unit_info>*)arg;
    copy_rtp_payload(*vec, AVD_NALS, frame);
    (void)uvgrtp::frame::dealloc_frame(frame);
}