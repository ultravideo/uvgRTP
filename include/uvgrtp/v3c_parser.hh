#pragma once

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

constexpr int    V3C_HDR_LEN         = 4; // 32 bits for v3c unit header

constexpr uint8_t     VVC_SC         = 0x00000001; // VVC Start Code

struct profile_tier_level {
    uint8_t ptl_tier_flag = 0;
    uint8_t ptl_profile_codec_group_idc = 0;
    uint8_t ptl_profile_toolset_idc = 0;
    uint8_t ptl_profile_reconstruction_idc = 0;
    uint8_t ptl_max_decodes_idc = 0;
    uint8_t ptl_level_idc = 0;
    uint8_t ptl_num_sub_profiles = 0;
    bool ptl_extended_sub_profile_flag = 0;
    std::vector<uint64_t> ptl_sub_profile_idc = {};
    bool ptl_toolset_constraints_present_flag = 0;
};
struct parameter_set {
    profile_tier_level ptl;
    uint8_t vps_v3c_parameter_set_id = 0;
    uint8_t vps_atlas_count_minus1 = 0;
};
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

struct v3c_unit_info {
    v3c_unit_header header;
    std::vector<nal_info> nal_infos = {};
    char* buf;
    uint64_t ptr = 0;
    bool ready = false;
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

uint32_t combineBytes(uint8_t byte1, uint8_t byte2, uint8_t byte3, uint8_t byte4);
uint32_t combineBytes(uint8_t byte1, uint8_t byte2, uint8_t byte3);
uint32_t combineBytes(uint8_t byte1, uint8_t byte2);
void convert_size_little_endian(uint32_t in, uint8_t* out, size_t output_size);
void convert_size_big_endian(uint32_t in, uint8_t* out, size_t output_size);

uint64_t get_size(std::string filename);
char* get_cmem(std::string filename, const size_t& len);

// ad is for AD and CAD substreams, vd is for all VD substreams
bool mmap_v3c_file(char* cbuf, uint64_t len, v3c_file_map &mmap);
void parse_v3c_header(v3c_unit_header &hdr, char* buf, uint64_t ptr);

void parse_vps_ptl(profile_tier_level &ptl, char* buf, uint64_t ptr);

