#pragma once

#include <iostream>
#include <fstream>
#include <cstring>
#include <vector>
#include <string>

// vuh_unit_type definitions:
constexpr int    V3C_VPS = 0; // V3C parameter set
constexpr int    V3C_AD  = 1; // Atlas data
constexpr int    V3C_OVD = 2; // Occupancy video data
constexpr int    V3C_GVD = 3; // Geometry video data
constexpr int    V3C_AVD = 4; // Attribute video data
constexpr int    V3C_PVD = 5; // Packed video data 
constexpr int    V3C_CAD = 6; // Common atlas data 

constexpr int    CODEC_AVC           = 0; // AVC Progressive High
constexpr int    CODEC_HEVC_MAIN10   = 1; // HEVC Main10
constexpr int    CODEC_HEVC444       = 2; // HEVC444
constexpr int    CODEC_VVC_MAIN10    = 3; // VVC Main10 

constexpr int    V3C_HDR_LEN         = 4; // 32 bits for v3c unit header

constexpr uint8_t     VVC_SC         = 0x00000001; // VVC Start Code

struct profile_tier_level {
    uint8_t ptl_tier_flag = 0;
    uint8_t ptl_profile_codec_group_idc = 0;
};
struct vuh_vps {
    profile_tier_level ptl;
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
        vuh_vps vps;
        vuh_ad ad;
        vuh_ovd ovd;
        vuh_gvd gvd;
        vuh_avd avd;
        vuh_pvd pvd;
        vuh_cad cad;
    };
};

uint32_t combineBytes(uint8_t byte1, uint8_t byte2, uint8_t byte3, uint8_t byte4);
uint32_t combineBytes(uint8_t byte1, uint8_t byte2, uint8_t byte3);
uint32_t combineBytes(uint8_t byte1, uint8_t byte2);

uint64_t get_size(std::string filename);
char* get_cmem(std::string filename, const size_t& len);

// ad is for AD and CAD substreams, vd is for all VD substreams
bool mmap_v3c_file(char* cbuf, uint64_t len, vuh_vps& param, std::vector<std::pair<uint64_t, uint64_t>>& ad,
    std::vector<std::pair<uint64_t, uint64_t>>& vd);
void parse_v3c_header(v3c_unit_header &hdr, char* buf, uint64_t ptr);