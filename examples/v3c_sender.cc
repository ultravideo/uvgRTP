#include <uvgrtp/lib.hh>
#include "v3c/v3c_util.hh"

#include <iostream>
#include <fstream>
#include <cstring>
#include <vector>
#include <string>
#include <atomic>

constexpr char REMOTE_ADDRESS[] = "127.0.0.1";

// Path to the V-PCC file that you want to send
std::string PATH = "longdress.vpcc";
void sender_func(uvgrtp::media_stream* stream, const char* cbuf, const std::vector<v3c_unit_info> &units, rtp_flags_t flags, int fmt);

std::atomic<uint64_t> bytes_sent;
int main(void)
{
    /* This example demonstrates transmitting a file that is in the V3C sample stream format via uvgRTP. It can be used to transmit
     * V-PCC encoded files, but with minor modifications (addition of V3C_CAD and V3C_PVD streams) it can also be used for MIV
     * encoded files.
     * 
     * The V3C sample stream contains a V3C sample stream header byte and multiple V3C Units with their sizes specified before
     * each unit. A V3C Unit then contains a V3C header and Atlas or Video NAL units, depending on the V3C unit type. Video
     * data can be either AVC, HEVC or VVC encoded. By default this example uses HEVC encoding. Using AVC or VVC only requires
     * you to set the media streams payload format accordingly.
     * 
     * The process of sending and receiving a V3C sample stream via uvgRTP contains the following steps:
     * Parse file and extract locations and sizes of NAL units -> Send NAL units in separate uvgRTP media streams
     * -> Receive NAL units -> Reconstruct V3C Sample Stream
     * 
     * In this example there are in total 5 media streams, one for each component:
     * 1. Parameter set stream (NOTE: Parameter set should be carried via a signaling protocol such as SDP at the start of the
          stream. In this demonstration, it is only transmitted using an RTP stream for simplicity.)
     * 2. Atlas stream
     * 3. Occupancy Video stream
     * 4. Geometry Video stream
     * 5. Attribute Video Stream
     * 
     * There is also the possibility to have two more streams: Packed Video and Common Atlas Data. These are not included in
     * this example as V-PCC files don't have them. If you need these (MIV), it is easy to add them
     *
     A few notes for users:
     - This demonstration expects there to be no packet loss. If some NAL units are lost in transmission,
       issues may occur in the reconstruction
     - Please also read the documentation on v3c_receiver.cc before testing this example.
     - The information in V3C unit headers is also supposed to be transmitted once at the start of the stream via SDP. For
       simplicity, in this example they are just deduced at the receiver.
     - Remember to use RTP_NO_H26X_SCL flag when sending video frames, as there is no start codes in the video sub-streams
     - v3c_sender and v3c_receiver programs use common functions defined in v3c_util. */

    bytes_sent = 0;
    v3c_file_map mmap;

    /* Read the file and its size */
    uint64_t len = get_size(PATH);
    if (len == 0) {
        return EXIT_FAILURE;
    }
    char* cbuf = get_cmem(PATH);
    std::cout << "Parsing V3C file, size " << len << std::endl;

    /* Map the locations and sizes of Atlas and video NAL units with the mmap_v3c_file function */
    mmap_v3c_file(cbuf, len, mmap);

    std::cout << "Sending V3C NAL units via uvgRTP" << std::endl;

    /* Create the necessary uvgRTP media streams */
    uvgrtp::context ctx;
    uvgrtp::session* sess = ctx.create_session(REMOTE_ADDRESS, REMOTE_ADDRESS);

    int flags = 0;
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

    std::cout << "Sending finished, " << bytes_sent << " bytes sent" << std::endl;

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
            bytes_sent += i.size;
        }
    }
}
