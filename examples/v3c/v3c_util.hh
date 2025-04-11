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
    V3C_CAD    = 6, // Common atlas data
    V3C_UNDEF  = -1 // Unit type not defined
};

enum CODEC {
    CODEC_AVC           = 0, // AVC Progressive High
    CODEC_HEVC_MAIN10   = 1, // HEVC Main10
    CODEC_HEVC444       = 2, // HEVC444
    CODEC_VVC_MAIN10    = 3 // VVC Main10 
};

enum class INFO_FMT {
  LOGGING, // Logging printout in a human readable format
  PARAM    // Print relevant parameters directly to c++ expressions
};

constexpr int V3C_HDR_LEN = 4; // 32 bits for v3c unit header
constexpr int SAMPLE_STREAM_HDR_LEN = 1; // 8 bits for sample stream headers
constexpr uint8_t DEFAULT_ATLAS_NAL_SIZE_PRECISION = 2;
constexpr uint8_t DEFAULT_VIDEO_NAL_SIZE_PRECISION = 4;

constexpr uint8_t MAX_V3C_SIZE_PREC = 8;

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

    v3c_unit_header() {
      std::memset(this, 0, sizeof(v3c_unit_header));
    }

    v3c_unit_header(const uint8_t unit_type) : v3c_unit_header() {
      this->vuh_unit_type = unit_type;
    }
};

struct nal_info {
    uint64_t location   = 0; // Start position of the NAL unit
    uint64_t size       = 0; // Size of the NAL unit
    char* buf = nullptr;     // Used on receiving end for temporary storage of the received NAL unit
};

/* A v3c_unit_info contains all the required information of a V3C unit
 - nal_info struct holds the format(Atlas, H264, H265, H266), start position and size of the NAL unit
 - With this info you can send the data via different uvgRTP media streams. */
struct v3c_unit_info {
    uint8_t sample_stream_nal_precision;
    v3c_unit_header header;
    std::vector<nal_info> nal_infos = {};
    //char* buf; // (used on the receiving end)
    uint64_t ptr = 0; // (used on the receiving end) total size of the received NAL units in a V3C unit
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

/* Information about the bitstream that should not be included in the rtp stream, but send through other means to the receiver
    - Sample stream headers (precision)
    - Number of GoFs (?)
    - Number of NALs in GoF
    - Other header info (?)
*/
struct bitstream_info {
  uint8_t v3c_sample_stream_precision = MAX_V3C_SIZE_PREC;
  std::map<V3C_UNIT_TYPE, uint8_t> nal_sample_stream_precision = {};
  bool variable_nal_precision = false;
  uint64_t num_GOF = 0;
  std::map<V3C_UNIT_TYPE, uint64_t> num_nal = {};
  bool variable_nal_num = false;

  bitstream_info() {
    this->nal_sample_stream_precision[V3C_VPS] = 0;
    this->nal_sample_stream_precision[V3C_AD ] = DEFAULT_ATLAS_NAL_SIZE_PRECISION;
    this->nal_sample_stream_precision[V3C_OVD] = DEFAULT_VIDEO_NAL_SIZE_PRECISION;
    this->nal_sample_stream_precision[V3C_GVD] = DEFAULT_VIDEO_NAL_SIZE_PRECISION;
    this->nal_sample_stream_precision[V3C_AVD] = DEFAULT_VIDEO_NAL_SIZE_PRECISION;
    this->nal_sample_stream_precision[V3C_PVD ] = DEFAULT_VIDEO_NAL_SIZE_PRECISION;
    this->nal_sample_stream_precision[V3C_CAD] = DEFAULT_ATLAS_NAL_SIZE_PRECISION;
    this->nal_sample_stream_precision[V3C_UNDEF] = 0;

    this->num_nal[V3C_VPS] = 0;
    this->num_nal[V3C_AD] = 0;
    this->num_nal[V3C_OVD] = 0;
    this->num_nal[V3C_GVD] = 0;
    this->num_nal[V3C_AVD] = 0;
    this->num_nal[V3C_PVD] = 0;
    this->num_nal[V3C_CAD] = 0;
    this->num_nal[V3C_UNDEF] = 0;
  }
};

/* Write bitstream info to given output stream
    fmt: What format should be used to print info (see INFO_FMT enum)
*/
void write_out_bitstream_info(bitstream_info& info, std::ostream& out_stream, INFO_FMT fmt=INFO_FMT::LOGGING);

uint64_t combineBytes(const uint8_t *const bytes, const uint8_t num_bytes);
void convert_size_big_endian(uint64_t in, uint8_t* out, size_t output_size);

// Get sizes of different v3c unit types
template <V3C_UNIT_TYPE E>
uint64_t get_v3c_unit_size(const v3c_unit_info &info);
uint64_t get_v3c_unit_size(const v3c_unit_info &info, const uint8_t unit_type);
template <>
uint64_t get_v3c_unit_size<V3C_VPS>(const v3c_unit_info &info);
template <>
uint64_t get_v3c_unit_size<V3C_AD>(const v3c_unit_info &info);
template <>
uint64_t get_v3c_unit_size<V3C_OVD>(const v3c_unit_info &info);
template <>
uint64_t get_v3c_unit_size<V3C_GVD>(const v3c_unit_info &info);
template <>
uint64_t get_v3c_unit_size<V3C_AVD>(const v3c_unit_info &info);

// Get size of a file in bytes
uint64_t get_size(std::string filename);

// Get a pointer to a file  
char* get_cmem(std::string filename);

// Memory map a V3C file
bool mmap_v3c_file(char* cbuf, uint64_t len, v3c_file_map &mmap, bitstream_info& info);

// Parse a V3C header into mmap
void parse_v3c_header(v3c_unit_header &hdr, char* buf, uint64_t ptr);

// Initialize a media stream for all 5 components of a V3C Stream
v3c_streams init_v3c_streams(uvgrtp::session* sess, uint16_t src_port, uint16_t dst_port, int flags, bool rec);

// Initialize a memory map of a V3C file
v3c_file_map init_mmap(uint8_t atlas_nal_precision = DEFAULT_ATLAS_NAL_SIZE_PRECISION, uint8_t video_nal_precision = DEFAULT_VIDEO_NAL_SIZE_PRECISION);

// Used in receiver_hooks to copy the received data
void copy_rtp_payload(std::vector<v3c_unit_info>* units, uint64_t max_size, uvgrtp::frame::rtp_frame* frame);

// Combine a complete V3C unit from received NAL units
void create_v3c_unit(v3c_unit_info& current_unit, char* buf, uint64_t& ptr, uint8_t v3c_precision);

// Reconstruct a whole GOF from V3C Units
uint64_t reconstruct_v3c_gof(bool hdr_byte, char* &buf, uint64_t& ptr, v3c_file_map& mmap, uint64_t index, uint8_t v3c_precision);

// Check if there is a complete GOF in the memory map
bool is_gof_ready(uint64_t index, v3c_file_map& mmap);

// Mark last nalu infos as ready. May cause issues if all data has not been received yet. Can be used to "flush" end of stream
void finalize_gof(v3c_file_map& mmap);

uint64_t get_gof_size(bool hdr_byte, uint64_t index, v3c_file_map& mmap, uint8_t v3c_precision);