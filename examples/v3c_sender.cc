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
std::string PATH = "C:\\Users\\ngheta\\Documents\\v3c_test_seq_2.vpcc";

void sender_func(uvgrtp::media_stream* stream, const char* cbuf, const std::vector<v3c_unit_info> &units, rtp_flags_t flags, int fmt);

int main(void)
{
    std::cout << "Parsing V3C file" << std::endl;

    /* A V3C Sample stream is divided into 6 types of 'sub-bitstreams' + parameters.
    - The nal_map holds nal_info structs
    - nal_info struct holds the format(Atlas, H264, H265, H266), start position and size of the NAL unit
    - With this info you can send the data via different uvgRTP media streams. Usually 2 streams, one in RTP_FORMAT_ATLAS for
      Atlas NAL units and a second one for the video NAL units in the correct format 
      
      Note: Use RTP_NO_H26X_SCL when sending video frames, as there is no start codes in the video sub-streams */

    v3c_file_map mmap;

    /* Fetch the file and its size */
    uint64_t len = get_size(PATH);
    char* cbuf = get_cmem(PATH);

    /* Map the locations and sizes of Atlas and video NAL units with the mmap_v3c_file function */
    mmap_v3c_file(cbuf, len, mmap);

    std::cout << "Sending V3C NAL units via uvgRTP" << std::endl;

    /* Create the necessary uvgRTP media streams */
    uvgrtp::context ctx;
    uvgrtp::session* sess = ctx.create_session(REMOTE_ADDRESS, REMOTE_ADDRESS);

    int flags = RCE_SEND_ONLY;
    v3c_streams streams = init_v3c_streams(sess, 8892, 8890, flags, false);

    /* Start sending data */
    std::unique_ptr<std::thread> vps_thread =
        std::unique_ptr<std::thread>(new std::thread(sender_func, streams.vps, cbuf, mmap.vps_units, RTP_NO_FLAGS, V3C_VPS));

    std::unique_ptr<std::thread> ad_thread =
        std::unique_ptr<std::thread>(new std::thread(sender_func, streams.ad, cbuf, mmap.ad_units, RTP_NO_FLAGS, V3C_AD));

    std::unique_ptr<std::thread> ovd_thread =
        std::unique_ptr<std::thread>(new std::thread(sender_func, streams.ovd, cbuf, mmap.ovd_units, RTP_NO_H26X_SCL, V3C_OVD));

    std::unique_ptr<std::thread> gvd_thread =
        std::unique_ptr<std::thread>(new std::thread(sender_func, streams.gvd, cbuf, mmap.gvd_units, RTP_NO_H26X_SCL, V3C_GVD));

    std::unique_ptr<std::thread> avd_thread =
        std::unique_ptr<std::thread>(new std::thread(sender_func, streams.avd, cbuf, mmap.avd_units, RTP_NO_H26X_SCL, V3C_AVD));

    if (vps_thread && vps_thread->joinable())
    {
        vps_thread->join();
    }
    if (ad_thread && ad_thread->joinable())
    {
        ad_thread->join();
    }
    if (ovd_thread && ovd_thread->joinable())
    {
        ovd_thread->join();
    }
    if (gvd_thread && gvd_thread->joinable())
    {
        gvd_thread->join();
    }
    if (avd_thread && avd_thread->joinable())
    {
        avd_thread->join();
    }

    sess->destroy_stream(streams.vps);
    sess->destroy_stream(streams.ad);
    sess->destroy_stream(streams.ovd);
    sess->destroy_stream(streams.gvd);
    sess->destroy_stream(streams.avd);


    std::cout << "Sending finished"  << std::endl;

    if (sess)
    {
        // Session must be destroyed manually
        ctx.destroy_session(sess);
    }
    return EXIT_SUCCESS;
}

void sender_func(uvgrtp::media_stream* stream, const char* cbuf, const std::vector<v3c_unit_info> &units, rtp_flags_t flags, int fmt)
{
    for (auto& p : units) {
        for (auto i : p.nal_infos) {
            rtp_error_t ret = RTP_OK;
            //std::cout << "Sending NAL unit in location " << i.location << " with size " << i.size << std::endl;
            ret = stream->push_frame((uint8_t*)cbuf + i.location, i.size, flags);
            if (ret != RTP_OK) {
                std::cout << "Failed to send RTP frame!" << std::endl;
            }
        }
    }

    
}
