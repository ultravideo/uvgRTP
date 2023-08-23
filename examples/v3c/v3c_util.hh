#include <uvgrtp/lib.hh>

#include <iostream>
#include <fstream>
#include <cstring>
#include <vector>
#include <string>

// vuh_unit_type definitions:
enum V3C_UNIT_TYPE {
    V3C_VPS    = 0, // V3C parameter set
    V3C_AD     = 1, // Atlas data
    V3C_OVD    = 2, // Occupancy video data
    V3C_GVD    = 3, // Geometry video data
    V3C_AVD    = 4, // Attribute video data
    V3C_PVD    = 5, // Packed video data 
    V3C_CAD    = 6 // Common atlas data 
};

enum CODEC {
    CODEC_AVC           = 0, // AVC Progressive High
    CODEC_HEVC_MAIN10   = 1, // HEVC Main10
    CODEC_HEVC444       = 2, // HEVC444
    CODEC_VVC_MAIN10    = 3 // VVC Main10 
};

constexpr int V3C_HDR_LEN = 4; // 32 bits for v3c unit header

// These are signaled to the receiver one way or the other, for example SDP
constexpr uint8_t ATLAS_NAL_SIZE_PRECISION = 2;
constexpr uint8_t VIDEO_NAL_SIZE_PRECISION = 4;
constexpr uint8_t V3C_SIZE_PRECISION = 3;
constexpr int INPUT_BUFFER_SIZE = 40 * 1000 * 1000; // Received NAL units are copied to the input buffer


struct vuh_ad {
    uint8_t vuh_v3c_parameter_set_id = 0;
    uint8_t vuh_atlas_id = 0;
};
struct vuh_ovd {
    uint8_t vuh_v3c_parameter_set_id = 0;
    uint8_t vuh_atlas_id = 0;
};
struct vuh_gvd {
    uint8_t vuh_v3c_parameter_set_id = 0;
    uint8_t vuh_atlas_id = 0;
    uint8_t vuh_map_index = 0;
    bool vuh_auxiliary_video_flag = 0;
};
struct vuh_avd {
    uint8_t vuh_v3c_parameter_set_id = 0;
    uint8_t vuh_atlas_id = 0;
    uint8_t vuh_attribute_index = 0;
    uint8_t vuh_attribute_partition_index = 0;
    uint8_t vuh_map_index = 0;
    bool vuh_auxiliary_video_flag = 0;
};
struct vuh_pvd {
    uint8_t vuh_v3c_parameter_set_id = 0;
    uint8_t vuh_atlas_id = 0;
};
struct vuh_cad {
    uint8_t vuh_v3c_parameter_set_id = 0;
};

struct v3c_unit_header {
    uint8_t vuh_unit_type = 0;
    union {
        vuh_ad ad;
        vuh_ovd ovd;
        vuh_gvd gvd;
        vuh_avd avd;
        vuh_pvd pvd;
        vuh_cad cad;
    };
};

struct nal_info {
    uint64_t location   = 0; // Start position of the NAL unit
    uint64_t size       = 0; // Sie of the NAL unit
};

/* A v3c_unit_info contains all the required information of a V3C unit
 - nal_info struct holds the format(Atlas, H264, H265, H266), start position and size of the NAL unit
 - With this info you can send the data via different uvgRTP media streams. */
struct v3c_unit_info {
    v3c_unit_header header;
    std::vector<nal_info> nal_infos = {};
    char* buf; // (used on the receiving end)
    uint64_t ptr = 0; // (used on the receiving end)
    bool ready = false; // (used on the receiving end)
};

struct v3c_file_map {
    std::vector<v3c_unit_info> vps_units = {};
    std::vector<v3c_unit_info> ad_units = {};
    std::vector<v3c_unit_info> ovd_units = {};
    std::vector<v3c_unit_info> gvd_units = {};
    std::vector<v3c_unit_info> avd_units = {};
    std::vector<v3c_unit_info> pvd_units = {};
    std::vector<v3c_unit_info> cad_units = {};
};

struct v3c_streams {
    uvgrtp::media_stream* vps = nullptr;
    uvgrtp::media_stream* ad = nullptr;
    uvgrtp::media_stream* ovd = nullptr;
    uvgrtp::media_stream* gvd = nullptr;
    uvgrtp::media_stream* avd = nullptr;
};

uint32_t combineBytes(uint8_t byte1, uint8_t byte2, uint8_t byte3, uint8_t byte4);
uint32_t combineBytes(uint8_t byte1, uint8_t byte2, uint8_t byte3);
uint32_t combineBytes(uint8_t byte1, uint8_t byte2);
void convert_size_big_endian(uint32_t in, uint8_t* out, size_t output_size);

// Get size of a file in bytes
uint64_t get_size(std::string filename);

// Get a pointer to a file  
char* get_cmem(std::string filename);

// Memory map a V3C file
bool mmap_v3c_file(char* cbuf, uint64_t len, v3c_file_map &mmap);

// Parse a V3C header into mmap
void parse_v3c_header(v3c_unit_header &hdr, char* buf, uint64_t ptr);

// Initialize a media stream for all 5 components of a V3C Stream
v3c_streams init_v3c_streams(uvgrtp::session* sess, uint16_t src_port, uint16_t dst_port, int flags, bool rec);

// Initialize a memory map of a V3C file
v3c_file_map init_mmap();

// Used in receiver_hooks to copy the received data
void copy_rtp_payload(std::vector<v3c_unit_info>& units, uint64_t max_size, uvgrtp::frame::rtp_frame* frame);

// Combine a complete V3C unit from received NAL units
void create_v3c_unit(v3c_unit_info& current_unit, char* buf, uint64_t& ptr, uint64_t v3c_precision, uint32_t nal_precision);

// Reconstruct a whole GoP from V3C Units
uint64_t reconstruct_v3c_gop(bool hdr_byte, char* &buf, uint64_t& ptr, v3c_file_map& mmap, uint64_t index);

// Check if there is a complete GoP in the memory map
bool is_gop_ready(uint64_t index, v3c_file_map& mmap);
