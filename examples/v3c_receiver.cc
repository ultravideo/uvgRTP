
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
void pvd_receive_hook(void* arg, uvgrtp::frame::rtp_frame* frame);
void cad_receive_hook(void* arg, uvgrtp::frame::rtp_frame* frame);

bool is_gop_ready(uint64_t index, v3c_file_map &mmap);
void copy_rtp_payload(uint64_t max_size, v3c_unit_info& unit, uvgrtp::frame::rtp_frame* frame);
void create_v3c_unit(v3c_unit_info& current_unit, char* buf, uint64_t& ptr, uint64_t v3c_precision, uint32_t nal_precision);
uint64_t reconstruct_v3c_gop(bool hdr_byte, char* buf, uint64_t &ptr, v3c_file_map &mmap, uint64_t index);
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
    uvgrtp::media_stream* ad = sess->create_stream(8893, 8892, RTP_FORMAT_V3C, flags);
    uvgrtp::media_stream* ovd = sess->create_stream(8895, 8894, RTP_FORMAT_H265, flags);
    uvgrtp::media_stream* gvd = sess->create_stream(8897, 8896, RTP_FORMAT_H265, flags);
    uvgrtp::media_stream* avd = sess->create_stream(8899, 8898, RTP_FORMAT_H265, flags);
    uvgrtp::media_stream* pvd = sess->create_stream(9001, 9000, RTP_FORMAT_H265, flags);
    uvgrtp::media_stream* cad = sess->create_stream(9003, 9002, RTP_FORMAT_V3C, flags);
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
    pvd->install_receive_hook(nullptr, pvd_receive_hook);
    cad->install_receive_hook(nullptr, cad_receive_hook);


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
    sess->destroy_stream(pvd);
    sess->destroy_stream(cad);

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

uint64_t reconstruct_v3c_gop(bool hdr_byte, char* buf, uint64_t &ptr, v3c_file_map& mmap, uint64_t index)
{
    /* Calculate GoP size and intiialize the output buffer
    * 
    + 1 byte of Sample Stream Precision
    +----------------------------------------------------------------+
    + 4 bytes of V3C Unit size
    + x1 bytes of whole V3C VPS unit (incl. header)
    +----------------------------------------------------------------+
      Atlas V3C unit
    + 4 bytes for V3C Unit size
    + 4 bytes of V3C header
    + 1 byte of NAL Unit Size Precision (x1)
    + NALs count (x1 bytes of NAL Unit Size
    + x2 bytes of NAL unit payload)
    +----------------------------------------------------------------+
      Video V3C unit
    + 4 bytes for V3C Unit size
    + 4 bytes of V3C header
    + NALs count (4 bytes of NAL Unit Size
    + x2 bytes of NAL unit payload)
    +----------------------------------------------------------------+
                .
                .
                .
    +----------------------------------------------------------------+
      Video V3C unit
    + 4 bytes for V3C Unit size
    + 4 bytes of V3C header
    + NALs count (4 bytes of NAL Unit Size
    + x2 bytes of NAL unit payload)
    +----------------------------------------------------------------+ */
    uint8_t ATLAS_NAL_SIZE_PRECISION = 2;
    uint8_t VIDEO_NAL_SIZE_PRECISION = 4;
    uint64_t gop_size = 0;
    if (hdr_byte) {
        gop_size++; // Sample Stream Precision
    }
    uint64_t vps_size = mmap.vps_units.at(index).nal_infos.at(0).size; // incl. header
    uint64_t ad_size  = 4+4+1 + mmap.ad_units.at(index).ptr + mmap.ad_units.at(index).nal_infos.size() * ATLAS_NAL_SIZE_PRECISION;
    uint64_t ovd_size = 8 + mmap.ovd_units.at(index).ptr + mmap.ovd_units.at(index).nal_infos.size() * VIDEO_NAL_SIZE_PRECISION;
    uint64_t gvd_size = 8 + mmap.gvd_units.at(index).ptr + mmap.gvd_units.at(index).nal_infos.size() * VIDEO_NAL_SIZE_PRECISION;
    uint64_t avd_size = 8 + mmap.avd_units.at(index).ptr + mmap.avd_units.at(index).nal_infos.size() * VIDEO_NAL_SIZE_PRECISION;
    gop_size += vps_size + ad_size + ovd_size + gvd_size + avd_size;
    std::cout << "Initializing GoP buffer of " << gop_size << " bytes" << std::endl;
    //buf = new char[gop_size];
    std::cout << "start ptr is " << ptr << std::endl;

    // V3C Sample stream header
    if (hdr_byte) {
        std::cout << "Adding Sample Stream header byte" << std::endl;
        uint8_t first_byte = 64;
        buf[ptr] = first_byte;
        ptr++;
    }

    uint8_t v3c_unit_size_precision = 3;

    uint8_t* v3c_size_arr = new uint8_t[v3c_unit_size_precision];

    v3c_unit_info current_unit = mmap.vps_units.at(index); // Now processing VPS unit
    uint32_t v3c_size_int = (uint32_t)current_unit.nal_infos.at(0).size;

    // Write the V3C VPS unit size to the output buffer
    convert_size_big_endian(v3c_size_int, v3c_size_arr, v3c_unit_size_precision);
    memcpy(&buf[ptr], v3c_size_arr, v3c_unit_size_precision);
    ptr += v3c_unit_size_precision;

    // Write the V3C VPS unit payload to the output buffer
    memcpy(&buf[ptr], current_unit.buf, v3c_size_int);
    ptr += v3c_size_int;

    // Write out V3C AD unit
    current_unit = mmap.ad_units.at(index);
    create_v3c_unit(current_unit, buf, ptr, v3c_unit_size_precision, ATLAS_NAL_SIZE_PRECISION);

    // Write out V3C OVD unit
    current_unit = mmap.ovd_units.at(index);
    create_v3c_unit(current_unit, buf, ptr, v3c_unit_size_precision, VIDEO_NAL_SIZE_PRECISION);

    // Write out V3C GVD unit
    current_unit = mmap.gvd_units.at(index);
    create_v3c_unit(current_unit, buf, ptr, v3c_unit_size_precision, VIDEO_NAL_SIZE_PRECISION);

    // Write out V3C AVD unit
    current_unit = mmap.avd_units.at(index);
    create_v3c_unit(current_unit, buf, ptr, v3c_unit_size_precision, VIDEO_NAL_SIZE_PRECISION);
    std::cout << "end ptr is " << ptr << std::endl;

    return gop_size;
}

void create_v3c_unit(v3c_unit_info &current_unit, char* buf, uint64_t &ptr, uint64_t v3c_precision, uint32_t nal_precision)
{
    uint8_t v3c_type = current_unit.header.vuh_unit_type;

    // V3C unit size
    uint8_t* v3c_size_arr = new uint8_t[v3c_precision];
    uint32_t v3c_size_int = 4 + current_unit.ptr + (uint32_t)current_unit.nal_infos.size() * nal_precision;
    if (v3c_type == V3C_AD ||v3c_type == V3C_CAD) {
        v3c_size_int++; // NAL size precision for Atlas V3C units
    }
    convert_size_big_endian(v3c_size_int, v3c_size_arr, v3c_precision);
    memcpy(&buf[ptr], v3c_size_arr, v3c_precision);
    ptr += v3c_precision;

    // Next up create the V3C unit header
    uint8_t v3c_header[4] = {0, 0, 0, 0};

    // All V3C unit types have parameter_set_id in header
    uint8_t param_set_id = current_unit.header.ad.vuh_v3c_parameter_set_id;
    std::cout << "VUH typ: " << (uint32_t)v3c_type << " param set id " << (uint32_t)param_set_id << std::endl;
    v3c_header[0] = v3c_type << 3 | ((param_set_id & 0b1110) >> 1);
    v3c_header[1] = ((param_set_id & 0b1) << 7);

    // Only CAD does NOT have atlas_id
    if (v3c_type != V3C_CAD) {
        uint8_t atlas_id = current_unit.header.ad.vuh_atlas_id;
        v3c_header[1] = v3c_header[1] | ((atlas_id & 0b111111) << 1);
    }
    // GVD has map_index and aux_video_flag, then zeroes
    if (v3c_type == V3C_GVD) {
        uint8_t map_index = current_unit.header.gvd.vuh_map_index;
        bool auxiliary_video_flag = current_unit.header.gvd.vuh_auxiliary_video_flag;
        v3c_header[1] = v3c_header[1] | ((map_index & 0b1000) >> 3);
        v3c_header[2] = ((map_index & 0b111) << 5) | (auxiliary_video_flag << 4);
    }
    if (v3c_type == V3C_AVD) {
        uint8_t vuh_attribute_index = current_unit.header.avd.vuh_attribute_index;
        uint8_t vuh_attribute_partition_index = current_unit.header.avd.vuh_attribute_partition_index;
        uint8_t vuh_map_index = current_unit.header.avd.vuh_map_index;
        bool vuh_auxiliary_video_flag = current_unit.header.avd.vuh_auxiliary_video_flag;

        v3c_header[1] = v3c_header[1] | ((vuh_attribute_index & 0b1000000) >> 7);
        v3c_header[2] = ((vuh_attribute_index & 0b111111) << 2) | ((vuh_attribute_partition_index & 0b11000) >> 3);
        v3c_header[3] = ((vuh_attribute_partition_index & 0b111) << 5) | (vuh_map_index << 1) | (int)vuh_auxiliary_video_flag;
    }

    // Copy V3C header to outbut buffer
    memcpy(&buf[ptr], v3c_header, 4);
    ptr += 4;

    // For Atlas V3C units, set one byte for NAL size precision
    if (v3c_type == V3C_AD || v3c_type == V3C_CAD) {
        uint8_t nal_size_precision_arr = uint8_t((nal_precision - 1) << 5);
        memcpy(&buf[ptr], &nal_size_precision_arr, 1);
        ptr++;
    }
    // For Video V3C units, NAL size precision is always 4 bytes

    // Copy V3C unit NAL sizes and NAL units to output buffer
    for (auto& p : current_unit.nal_infos) {

        // Copy size
        uint8_t* nal_size_arr = new uint8_t[nal_precision];
        convert_size_big_endian(uint32_t(p.size), nal_size_arr, nal_precision);
        memcpy(&buf[ptr], nal_size_arr, nal_precision);
        ptr += nal_precision;

        // Copy NAL unit
        memcpy(&buf[ptr], &current_unit.buf[p.location], p.size);
        ptr += p.size;
    }

}

bool is_gop_ready(uint64_t index, v3c_file_map& mmap)
{
    if (mmap.vps_units.size() < index)
        return false;
    if (mmap.ad_units.size() < index || !mmap.ad_units.at(index-1).ready)
        return false;
    if (mmap.ovd_units.size() < index || !mmap.ovd_units.at(index-1).ready)
        return false;
    if (mmap.gvd_units.size() < index || !mmap.gvd_units.at(index-1).ready)
        return false;
    if (mmap.avd_units.size() < index || !mmap.avd_units.at(index-1).ready)
        return false;

    return true;
}


void copy_rtp_payload(uint64_t max_size, v3c_unit_info &unit, uvgrtp::frame::rtp_frame *frame)
{
    if (unit.nal_infos.size() <= max_size) {
        //std::cout << "Copy info " << std::endl;
        memcpy(&unit.buf[unit.ptr], frame->payload, frame->payload_len);

        unit.nal_infos.push_back({ unit.ptr, frame->payload_len });

        unit.ptr += frame->payload_len;
    }
    if (unit.nal_infos.size() == max_size) {
        unit.ready = true;
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


    if ((vec->end()-1)->nal_infos.size() == AD_NALS) {
        std::cout << "AD size  == 35, adding new v3c_unit " << std::endl;
        v3c_unit_header hdr = { V3C_AD };
        hdr.ad = { (uint8_t)vec->size(), 0 };
        v3c_unit_info info = { hdr, {}, new char[400 * 1000], 0, false};
        vec->push_back(info);
    }
    auto current = vec->end()-1;
    // GET THIS NUMBER 35 DYNAMICALLY
    copy_rtp_payload(AD_NALS, *current, frame);

    //std::cout << "Done with AD frame, num: " << current->nal_infos.size() << std::endl;

    (void)uvgrtp::frame::dealloc_frame(frame);
}
void ovd_receive_hook(void* arg, uvgrtp::frame::rtp_frame* frame)
{
    //std::cout << "Received OVD frame, size: " << frame->payload_len << " bytes" << std::endl;
    std::vector<v3c_unit_info>* vec = (std::vector<v3c_unit_info>*)arg;

    if ((vec->end() - 1)->nal_infos.size() == OVD_NALS) {
        std::cout << "OVD size  == 35, adding new v3c_unit " << std::endl;
        v3c_unit_header hdr = { V3C_OVD};
        hdr.ovd = {(uint8_t)vec->size(), 0};
        v3c_unit_info info = { hdr, {}, new char[400 * 1000], 0, false };
        vec->push_back(info);
    }
    auto current = vec->end() - 1;
    // GET THIS NUMBER 35 DYNAMICALLY
    copy_rtp_payload(OVD_NALS, *current, frame);
    (void)uvgrtp::frame::dealloc_frame(frame);
}
void gvd_receive_hook(void* arg, uvgrtp::frame::rtp_frame* frame)
{
    //std::cout << "Received GVD frame, size: " << frame->payload_len << " bytes" << std::endl;
    std::vector<v3c_unit_info>* vec = (std::vector<v3c_unit_info>*)arg;

    if ((vec->end() - 1)->nal_infos.size() == GVD_NALS) {
        std::cout << "GVD size  == 131, adding new v3c_unit " << std::endl;
        v3c_unit_header hdr = { V3C_GVD };
        hdr.gvd = { (uint8_t)vec->size(), 0, 0, 0 };
        v3c_unit_info info = { hdr, {}, new char[400 * 1000], 0, false };
        vec->push_back(info);
    }
    auto current = vec->end() - 1;
    // GET THIS NUMBER 35 DYNAMICALLY
    copy_rtp_payload(GVD_NALS, *current, frame);
    (void)uvgrtp::frame::dealloc_frame(frame);
}
void avd_receive_hook(void* arg, uvgrtp::frame::rtp_frame* frame)
{
    //std::cout << "Received AVD frame, size: " << frame->payload_len << " bytes" << std::endl;
    std::vector<v3c_unit_info>* vec = (std::vector<v3c_unit_info>*)arg;

    if ((vec->end() - 1)->nal_infos.size() == AVD_NALS) {
        std::cout << "AVD size  == 131, adding new v3c_unit " << std::endl;
        v3c_unit_header hdr = { V3C_AVD };
        hdr.avd = { (uint8_t)vec->size(), 0 };
        v3c_unit_info info = { hdr, {}, new char[40 * 1000 * 1000], 0, false };
        vec->push_back(info);
    }
    auto current = vec->end() - 1;
    // GET THIS NUMBER 35 DYNAMICALLY
    copy_rtp_payload(AVD_NALS, *current, frame);
    (void)uvgrtp::frame::dealloc_frame(frame);
}
void pvd_receive_hook(void* arg, uvgrtp::frame::rtp_frame* frame)
{
    //std::cout << "Received PVD frame, size: " << frame->payload_len << " bytes" << std::endl;

    (void)uvgrtp::frame::dealloc_frame(frame);
}
void cad_receive_hook(void* arg, uvgrtp::frame::rtp_frame* frame)
{
    //std::cout << "Received CAD frame, size: " << frame->payload_len << " bytes" << std::endl;

    (void)uvgrtp::frame::dealloc_frame(frame);
}