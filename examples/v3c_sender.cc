#include <uvgrtp/lib.hh>

#include <iostream>
#include <fstream>
#include <cstring>
#include <vector>
#include <string>

constexpr char REMOTE_ADDRESS[] = "127.0.0.1";
constexpr uint16_t REMOTE_PORT = 8890;

// the parameters of demostration
constexpr size_t PAYLOAD_LEN = 100;
constexpr int    AMOUNT_OF_TEST_PACKETS = 100;
constexpr auto   END_WAIT = std::chrono::seconds(5);

//std::string PATH = "C:\\Users\\ngheta\\Documents\\TMIV_A3_C_QP3.bit";
std::string PATH = "C:\\Users\\ngheta\\Documents\\test_seq3.vpcc";

int main(void)
{
    std::cout << "Parsing V3C file" << std::endl;

    /* A V3C Sample stream is divided into 6 types of 'sub-bitstreams' + parameters.
    - The ad_map vector holds the locations and sizes of Atlas NAL units in the file (both AD and CAD)
    - The vd_map holds the locations and sizes of all video V3C units (OVD, GVD, AVD, PVD)
    - First uint64_t is the start position of the unit, second is the size
    - With this info you can send the data via different uvgRTP media streams */

    std::vector<std::pair<uint64_t, uint64_t>> ad_map = {};
    std::vector<std::pair<uint64_t, uint64_t>> vd_map = {};

    uint64_t len = get_size(PATH);
    uint64_t ptr = 0;
    char* cbuf = nullptr;
    cbuf = get_cmem(PATH, len);

    vuh_vps parameters = {};
    mmap_v3c_file(cbuf, len, parameters, ad_map, vd_map);

    std::cout << "Sending Atlas NAL units via uvgRTP" << std::endl;

    uvgrtp::context ctx;
    uvgrtp::session* sess = ctx.create_session(REMOTE_ADDRESS, REMOTE_ADDRESS);
    int flags = RCE_SEND_ONLY;
    uvgrtp::media_stream* v3c = sess->create_stream(8890, 7790, RTP_FORMAT_V3C, flags);
    uvgrtp::media_stream* vid = nullptr;

    // Create the uvgRTP media stream with the correct RTP format
    if (parameters.ptl.ptl_profile_codec_group_idc == CODEC_AVC) {
        std::cout << "Video codec: AVC Progressive High" << std::endl;
        vid = sess->create_stream(8892, 7792, RTP_FORMAT_H264, flags);
    }
    else if (parameters.ptl.ptl_profile_codec_group_idc == CODEC_HEVC_MAIN10) {
        std::cout << "Video codec: HEVC Main10" << std::endl;
        vid = sess->create_stream(8892, 7792, RTP_FORMAT_H265, flags);
    }
    else if (parameters.ptl.ptl_profile_codec_group_idc == CODEC_HEVC444) {
        std::cout << "Video codec: HEVC444" << std::endl;
        vid = sess->create_stream(8892, 7792, RTP_FORMAT_H265, flags);
    }
    else if (parameters.ptl.ptl_profile_codec_group_idc == CODEC_VVC_MAIN10) {
        std::cout << "Video codec: VVC Main10" << std::endl;
        vid = sess->create_stream(8892, 7792, RTP_FORMAT_H266, flags);
    }
        

    uint64_t v3c_bytes_sent = 0;
    uint64_t avc_bytes_sent = 0;
    uint64_t send_ptr = 0;
    for (auto p : ad_map)
    {
        std::cout << "Sending frame in location " << p.first << " with size " << p.second << std::endl;

        if (v3c->push_frame((uint8_t*)cbuf + p.first, p.second, RTP_NO_FLAGS) != RTP_OK)
        {
            std::cout << "Failed to send RTP frame!" << std::endl;
        }
        else {
            v3c_bytes_sent += p.second;
        }
    }
    for (auto p : vd_map)
    {
        std::cout << "Sending frame in location " << p.first << " with size " << p.second << std::endl;

        if (vid->push_frame((uint8_t*)cbuf + p.first, p.second, RTP_NO_FLAGS) != RTP_OK)
        {
            std::cout << "Failed to send RTP frame!" << std::endl;
        }
        else {
            avc_bytes_sent += p.second;
        }
    }

    std::cout << "V3C Sending finished. Total bytes sent " << v3c_bytes_sent << std::endl;
    std::cout << "AVC Sending finished. Total bytes sent " << avc_bytes_sent << std::endl;

    sess->destroy_stream(v3c);
    sess->destroy_stream(vid);
    if (sess)
    {
        // Session must be destroyed manually
        ctx.destroy_session(sess);
    }
    return EXIT_SUCCESS;
}
