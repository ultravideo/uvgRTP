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

std::string PATH = "C:\\Users\\ngheta\\Documents\\TMIV_A3_C_QP3.bit";
//std::string PATH = "C:\\Users\\ngheta\\Documents\\test_seq3.vpcc";

int main(void)
{
    std::cout << "Parsing V3C file" << std::endl;

    /* A V3C Sample stream is divided into 6 types of 'sub-bitstreams' + parameters.
    - The nal_map holds nal_info structs
    - nal_info struct holds the format(Atlas, H264, H265, H266), start position and size of the NAL unit
    - With this info you can send the data via different uvgRTP media streams. Usually 2 streams, one in RTP_FORMAT_V3C for
      Atlas NAL units and a second one for the video NAL units in the correct format 
      
      Note: Use RTP_NO_H26X_SCL when sending video frames, as there is no start codes in the video sub-streams */

    std::vector<nal_info> nal_map = {};
    std::vector<vuh_vps> vps_map = {};

    /* Fetch the file and its size */
    uint64_t len = get_size(PATH);
    uint64_t ptr = 0;
    char* cbuf = nullptr;
    cbuf = get_cmem(PATH, len);

    /* Map the locations and sizes of Atlas and video NAL units with the mmap_v3c_file function */
    mmap_v3c_file(cbuf, len, nal_map, vps_map);

    std::cout << "Sending Atlas NAL units via uvgRTP" << std::endl;

    /* Create the necessary uvgRTP media streams */
    uvgrtp::context ctx;
    uvgrtp::session* sess = ctx.create_session(REMOTE_ADDRESS, REMOTE_ADDRESS);
    int flags = RCE_SEND_ONLY;
    uvgrtp::media_stream* v3c = sess->create_stream(8890, 7790, RTP_FORMAT_V3C, flags);
    rtp_format_t video_format = RTP_FORMAT_GENERIC;

    // Create the uvgRTP media stream with the correct RTP format
    if (vps_map.begin()->ptl.ptl_profile_codec_group_idc == CODEC_AVC) {
        std::cout << "Video codec: AVC Progressive High" << std::endl;
        video_format = RTP_FORMAT_H264;
    }
    else if (vps_map.begin()->ptl.ptl_profile_codec_group_idc == CODEC_HEVC_MAIN10) {
        std::cout << "Video codec: HEVC Main10" << std::endl;
        video_format = RTP_FORMAT_H265;
    }
    else if (vps_map.begin()->ptl.ptl_profile_codec_group_idc == CODEC_HEVC444) {
        std::cout << "Video codec: HEVC444" << std::endl;
        video_format = RTP_FORMAT_H265;
    }
    else if (vps_map.begin()->ptl.ptl_profile_codec_group_idc == CODEC_VVC_MAIN10) {
        std::cout << "Video codec: VVC Main10" << std::endl;
        video_format = RTP_FORMAT_H266;
    }
    uvgrtp::media_stream* vid = sess->create_stream(8892, 7792, video_format, flags);


    uint64_t bytes_sent = 0;
    uint64_t send_ptr = 0;

    /* Start sending data */
    for (auto p : nal_map)
    {
        rtp_error_t ret = RTP_OK;
        if (p.format == V3C_AD || p.format == V3C_CAD) {
            std::cout << "Sending V3C NAL unit in location " << p.location << " with size " << p.size << std::endl;
            ret = v3c->push_frame((uint8_t*)cbuf + p.location, p.size, RTP_NO_FLAGS);

        }
        else {
            std::cout << "Sending video NAL unit in location " << p.location << " with size " << p.size << std::endl;
            ret = vid->push_frame((uint8_t*)cbuf + p.location, p.size, RTP_NO_H26X_SCL);
        }
        if (ret == RTP_OK) {
            bytes_sent += p.size;
        }
        else {
            std::cout << "Failed to send RTP frame!" << std::endl;
        }
    }

    std::cout << "Sending finished. Total bytes sent " << bytes_sent << std::endl;

    sess->destroy_stream(v3c);
    sess->destroy_stream(vid);
    if (sess)
    {
        // Session must be destroyed manually
        ctx.destroy_session(sess);
    }
    return EXIT_SUCCESS;
}
