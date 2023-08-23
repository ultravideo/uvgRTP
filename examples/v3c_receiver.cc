
#include <uvgrtp/lib.hh>

#include <thread>
#include <iostream>
#include <fstream>

/* There are two main ways of getting received RTP frames from uvgRTP.
 * This example demonstrates the usage of hook function to receive RTP frames.
 *
 * The advantage of using a hook function is minimal CPU usage and delay between
 * uvgRTP receiving the frame and application processing the frame. When using
 * the hook method, the application must take care that it is not using the hook
 * function for heavy processing since this may block RTP frame reception.
 *
 * Hook based frame reception is generally recommended for most serious applications,
 * but there can be situations where polling method is better, especially if performance
 * is not a huge concern or if there needs to be tight control when the frame is
 * received by the application.
 *
 * This example only implements the receiving, but it can be used together with the
 * sending example to test the functionality.
 */

 // parameters for this test. You can change these to suit your network environment
constexpr uint16_t LOCAL_PORT = 8890;

constexpr char LOCAL_ADDRESS[] = "127.0.0.1";

// This example runs for 5 seconds
constexpr auto RECEIVE_TIME_S = std::chrono::seconds(5);

void vps_receive_hook(void* arg, uvgrtp::frame::rtp_frame* frame);
void ad_receive_hook(void* arg, uvgrtp::frame::rtp_frame* frame);
void ovd_receive_hook(void* arg, uvgrtp::frame::rtp_frame* frame);
void gvd_receive_hook(void* arg, uvgrtp::frame::rtp_frame* frame);
void avd_receive_hook(void* arg, uvgrtp::frame::rtp_frame* frame);

void copy_rtp_payload(std::vector<v3c_unit_info> &units, uint64_t max_size, uvgrtp::frame::rtp_frame* frame);
uint64_t vps_count;

constexpr int VPS_NALS = 1;
constexpr int AD_NALS = 35;
constexpr int OVD_NALS = 35;
constexpr int GVD_NALS = 131;
constexpr int AVD_NALS = 131;
std::string PATH = "C:\\Users\\ngheta\\Documents\\v3c_test_seq_2.vpcc";

int main(void)
{
    std::cout << "Starting uvgRTP RTP receive hook example" << std::endl;

    /* Fetch the original file and its size */
    uint64_t len = get_size(PATH);
    std::cout << "File size " << len << std::endl;

    char* original_buf = nullptr;
    original_buf = get_cmem(PATH, len);

    uvgrtp::context ctx;
    uvgrtp::session* sess = ctx.create_session(LOCAL_ADDRESS, LOCAL_ADDRESS);
    int flags = RCE_RECEIVE_ONLY | RCE_NO_H26X_PREPEND_SC;

    // Create the uvgRTP media streams with the correct RTP format
    uvgrtp::media_stream* vps = sess->create_stream(8891, 8890, RTP_FORMAT_GENERIC, flags);
    uvgrtp::media_stream* ad = sess->create_stream(8893, 8892, RTP_FORMAT_ATLAS, flags);
    uvgrtp::media_stream* ovd = sess->create_stream(8895, 8894, RTP_FORMAT_H265, flags);
    uvgrtp::media_stream* gvd = sess->create_stream(8897, 8896, RTP_FORMAT_H265, flags);
    uvgrtp::media_stream* avd = sess->create_stream(8899, 8898, RTP_FORMAT_H265, flags);
    avd->configure_ctx(RCC_RING_BUFFER_SIZE, 40*1000*1000);

    char* out_buf = new char[len];
    v3c_file_map mmap = {};


    v3c_unit_header hdr = { V3C_AD };
    hdr.ad = { 0, 0 };
    v3c_unit_info unit = { hdr, {}, new char[40 * 1000 * 1000], 0, false };
    mmap.ad_units.push_back(unit);

    hdr = { V3C_OVD };
    hdr.ovd = { 0, 0 };
    unit = { hdr, {}, new char[40 * 1000 * 1000], 0, false };
    mmap.ovd_units.push_back(unit);

    hdr = { V3C_GVD };
    hdr.gvd = { 0, 0 };
    unit = { hdr, {}, new char[40 * 1000 * 1000], 0, false };
    mmap.gvd_units.push_back(unit);

    hdr = { V3C_AVD };
    hdr.avd = { 0, 0 };
    unit = { hdr, {}, new char[40 * 1000 * 1000], 0, false };
    mmap.avd_units.push_back(unit);


    vps->install_receive_hook(&mmap.vps_units, vps_receive_hook);
    ad->install_receive_hook(&mmap.ad_units, ad_receive_hook);
    ovd->install_receive_hook(&mmap.ovd_units, ovd_receive_hook);
    gvd->install_receive_hook(&mmap.gvd_units, gvd_receive_hook);
    avd->install_receive_hook(&mmap.avd_units, avd_receive_hook);


    std::cout << "Waiting incoming packets for " << RECEIVE_TIME_S.count() << " s" << std::endl;
    uint64_t ngops = 1;
    uint64_t bytes = 0;
    uint64_t ptr = 0;
    bool hdb = true;
    while (ngops <= 1) {
        if (is_gop_ready(ngops, mmap)) {
            std::cout << "Full GoP received, num: " << ngops << std::endl;
            bytes +=  reconstruct_v3c_gop(hdb, out_buf, ptr, mmap, ngops - 1);
            std::cout << "File size " << bytes << std::endl;

            ngops++;
            hdb = false;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    std::this_thread::sleep_for(RECEIVE_TIME_S); // lets this example run for some time

    sess->destroy_stream(vps);
    sess->destroy_stream(ad);
    sess->destroy_stream(ovd);
    sess->destroy_stream(gvd);
    sess->destroy_stream(avd);

    ctx.destroy_session(sess);

    // compare files
    for (auto i = 0; i < len; ++i) {
        if (original_buf[i] != out_buf[i]) {
            std::cout << "Difference at " << i << std::endl;
            break;
        }
    }

    std::cout << "DONE " << std::endl;

    return EXIT_SUCCESS;
}

void copy_rtp_payload(std::vector<v3c_unit_info> &units, uint64_t max_size, uvgrtp::frame::rtp_frame *frame)
{
    if ((units.end() - 1)->nal_infos.size() == max_size) {
        std::cout << "AD size  == 35, adding new v3c_unit " << std::endl;
        v3c_unit_header hdr = {(units.end()-1)->header.vuh_unit_type};
        switch ((units.end()-1)->header.vuh_unit_type) {
            case V3C_AD: {
                hdr.ad = { (uint8_t)units.size(), 0 };
                v3c_unit_info info = { hdr, {}, new char[400 * 1000], 0, false };
                units.push_back(info);
                break;
            }        
            case V3C_OVD: {
                hdr.ovd = { (uint8_t)units.size(), 0 };
                v3c_unit_info info = { hdr, {}, new char[400 * 1000], 0, false };
                units.push_back(info);
                break;
            }
            case V3C_GVD: {
                hdr.gvd = { (uint8_t)units.size(), 0, 0, 0 };
                v3c_unit_info info = { hdr, {}, new char[400 * 1000], 0, false };
                units.push_back(info);
                break;
            }
            case V3C_AVD: {
                hdr.avd = { (uint8_t)units.size(), 0 };
                v3c_unit_info info = { hdr, {}, new char[40 * 1000 * 1000], 0, false };
                units.push_back(info);
                break;
            }
        }
    }
    auto &current = units.end() - 1;

    if (current->nal_infos.size() <= max_size) {
        memcpy(&current->buf[current->ptr], frame->payload, frame->payload_len);
        current->nal_infos.push_back({ current->ptr, frame->payload_len });
        current->ptr += frame->payload_len;
    }
    if (current->nal_infos.size() == max_size) {
        current->ready = true;
    }
}

void vps_receive_hook(void* arg, uvgrtp::frame::rtp_frame* frame)
{
    std::cout << "Received VPS frame, size: " << frame->payload_len << " bytes" << std::endl;

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

    //std::cout << "Done with AD frame, num: " << current->nal_infos.size() << std::endl;

    (void)uvgrtp::frame::dealloc_frame(frame);
}
void ovd_receive_hook(void* arg, uvgrtp::frame::rtp_frame* frame)
{
    //std::cout << "Received OVD frame, size: " << frame->payload_len << " bytes" << std::endl;
    std::vector<v3c_unit_info>* vec = (std::vector<v3c_unit_info>*)arg;

    copy_rtp_payload(*vec, OVD_NALS, frame);
    (void)uvgrtp::frame::dealloc_frame(frame);
}
void gvd_receive_hook(void* arg, uvgrtp::frame::rtp_frame* frame)
{
    //std::cout << "Received GVD frame, size: " << frame->payload_len << " bytes" << std::endl;
    std::vector<v3c_unit_info>* vec = (std::vector<v3c_unit_info>*)arg;

    copy_rtp_payload(*vec, GVD_NALS, frame);

    (void)uvgrtp::frame::dealloc_frame(frame);
}
void avd_receive_hook(void* arg, uvgrtp::frame::rtp_frame* frame)
{
    //std::cout << "Received AVD frame, size: " << frame->payload_len << " bytes" << std::endl;
    std::vector<v3c_unit_info>* vec = (std::vector<v3c_unit_info>*)arg;
    
    copy_rtp_payload(*vec, AVD_NALS, frame);
    (void)uvgrtp::frame::dealloc_frame(frame);
}