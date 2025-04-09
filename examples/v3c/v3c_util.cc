#include "v3c/v3c_util.hh"

#include <cassert>

uint64_t combineBytes(const uint8_t *const bytes, const uint8_t num_bytes) {
    uint64_t combined_out = 0;
    for(uint8_t i = 0; i < num_bytes; ++i){
        combined_out |= (static_cast<uint64_t>(bytes[i]) << (8 * (num_bytes - 1 - i)));
    }
    return combined_out;
}

void convert_size_big_endian(uint64_t in, uint8_t* out, size_t output_size) {
    for (size_t i = 0; i < output_size; ++i) {
        out[output_size - i - 1] = static_cast<uint8_t>(in >> (8 * i));
    }
}

bool mmap_v3c_file(char* cbuf, uint64_t len, v3c_file_map &mmap)
{
    uint64_t ptr = 0;

    // First byte is the file header
    uint8_t first_byte = cbuf[ptr];
    std::cout << "First byte " << uint32_t(first_byte) << std::endl;
    uint8_t v3c_size_precision = (first_byte >> 5) + 1;
    assert(0 < v3c_size_precision && v3c_size_precision <= 8 && "sample stream precision should be in range [1,8]");

    std::cout << "V3C size precision: " << (uint32_t)v3c_size_precision << std::endl;
    std::cout << std::endl;
    ++ptr;

    uint8_t v3c_size[MAX_V3C_SIZE_PREC] = {};
    uint8_t nal_size_precision = 0;
    while (true) {
        if (ptr >= len) {
            break;
        }
        // Read the V3C unit size 
        memcpy(v3c_size, &cbuf[ptr], v3c_size_precision);

        ptr += v3c_size_precision; // Jump over the V3C unit size bytes

        uint64_t combined_v3c_size = combineBytes(v3c_size, v3c_size_precision);

        // Inside v3c unit now
        std::cout << "Current V3C unit location " << ptr << ", size " << combined_v3c_size << std::endl;
        uint64_t v3c_ptr = ptr;

        // Next 4 bytes are the V3C unit header
        v3c_unit_header v3c_hdr = {};
        parse_v3c_header(v3c_hdr, cbuf, v3c_ptr);
        uint8_t vuh_t = v3c_hdr.vuh_unit_type;
        std::cout << "-- vuh_unit_type: " << (uint32_t)vuh_t << std::endl;
        v3c_unit_info unit = {0, v3c_hdr, {}};

        if (vuh_t == V3C_VPS) {
            // Parameter set contains no NAL units, skip over
            std::cout << "-- Parameter set V3C unit" << std::endl;
            nal_info nalu = {ptr, combined_v3c_size, nullptr};
            unit.nal_infos.push_back(nalu);
            mmap.vps_units.push_back(unit);
            ptr += combined_v3c_size;
            std::cout << std::endl;
            continue;
        }

        // Rest of the function goes inside the V3C unit payload and parses it into NAL units
        v3c_ptr += V3C_HDR_LEN; // Jump over 4 bytes of V3C unit header
        if (vuh_t == V3C_AD || vuh_t == V3C_CAD) {
            uint8_t nalu_first_byte = cbuf[v3c_ptr]; // Next up is 1 byte of NAL unit size precision
            unit.sample_stream_nal_precision = nal_size_precision = (nalu_first_byte >> 5) + 1;
            std::cout << "  -- Atlas NAL Sample stream, 1 byte for NAL unit size precision: " << (uint32_t)nal_size_precision << std::endl;
            ++v3c_ptr;
        }
        else {
            unit.sample_stream_nal_precision = nal_size_precision = 4;
            std::cout << "  -- Video NAL Sample stream, using NAL unit size precision of: " << (uint32_t)nal_size_precision << std::endl;
        }
        assert(0 < nal_size_precision && nal_size_precision <= 8 && "sample stream precision should be in range [1,8]");

        uint64_t amount_of_nal_units = 0;
        // Now start to parse the NAL sample stream
        while (true) {
            if (v3c_ptr >= (ptr + combined_v3c_size)) {
                break;
            }
            amount_of_nal_units++;
            uint64_t combined_nal_size = 0;
            
            combined_nal_size = combineBytes((uint8_t*)(&cbuf[v3c_ptr]), nal_size_precision);
            
            v3c_ptr += nal_size_precision;
            switch (vuh_t) {
            case V3C_AD:
            case V3C_CAD: {
                uint8_t atlas_nalu_t = (cbuf[v3c_ptr] & 0b01111110) >> 1;
                std::cout << "  -- v3c_ptr: " << v3c_ptr << ", NALU size: " << combined_nal_size << ", Atlas NALU type: " << (uint32_t)atlas_nalu_t << std::endl;
                break; }
            case V3C_OVD:
            case V3C_GVD:
            case V3C_AVD:
            case V3C_PVD:
                uint8_t h265_nalu_t = (cbuf[v3c_ptr] & 0b01111110) >> 1;
                std::cout << "  -- v3c_ptr: " << v3c_ptr << ", NALU size: " << combined_nal_size << ", HEVC NALU type: " << (uint32_t)h265_nalu_t << std::endl;
            }
            nal_info nalu = { v3c_ptr, combined_nal_size, nullptr };
            unit.nal_infos.push_back({ v3c_ptr, combined_nal_size, nullptr });
            v3c_ptr += combined_nal_size;

        }
        std::cout << "  -- Amount of NAL units in v3c unit: " << amount_of_nal_units << std::endl;
            
        switch (vuh_t) {
            case V3C_AD:
                mmap.ad_units.push_back(unit);
                break;
            case V3C_CAD:
                mmap.cad_units.push_back(unit);
                break;
            case V3C_OVD:
                mmap.ovd_units.push_back(unit);
                break;
            case V3C_GVD:
                mmap.gvd_units.push_back(unit);
                break;
            case V3C_AVD:
                mmap.avd_units.push_back(unit);
                break;
            case V3C_PVD:
                mmap.pvd_units.push_back(unit);
                break;
        }
        std::cout << std::endl;
        ptr += combined_v3c_size;
    }
    std::cout << "File parsed" << std::endl;
    return true;
}

void parse_v3c_header(v3c_unit_header &hdr, char* buf, uint64_t ptr)
{
    uint8_t vuh_unit_type = (buf[ptr] & 0b11111000) >> 3;
    hdr.vuh_unit_type = vuh_unit_type;

    uint8_t vuh_v3c_parameter_set_id = 0;;
    uint8_t vuh_atlas_id = 0;
    
    if (vuh_unit_type == V3C_AVD || vuh_unit_type == V3C_GVD ||
        vuh_unit_type == V3C_OVD || vuh_unit_type == V3C_AD ||
        vuh_unit_type == V3C_CAD || vuh_unit_type == V3C_PVD) {

        // 3 last bits from first byte and 1 first bit from second byte
        vuh_v3c_parameter_set_id = ((buf[ptr] & 0b111) << 1) | ((buf[ptr + 1] & 0b10000000) >> 7);
        std::cout << "-- vuh_v3c_parameter_set_id: " << (uint32_t)vuh_v3c_parameter_set_id << std::endl;
    }
    if (vuh_unit_type == V3C_AVD || vuh_unit_type == V3C_GVD ||
        vuh_unit_type == V3C_OVD || vuh_unit_type == V3C_AD ||
        vuh_unit_type == V3C_PVD) {

        // 6 middle bits from the second byte
        vuh_atlas_id = ((buf[ptr + 1] & 0b01111110) >> 1);
        std::cout << "-- vuh_atlas_id: " << (uint32_t)vuh_atlas_id << std::endl;
    }

    switch (hdr.vuh_unit_type) {
    case V3C_VPS: {
        break;
    }
    case V3C_AD:
        hdr.ad = {};
        hdr.ad.vuh_v3c_parameter_set_id = vuh_v3c_parameter_set_id;
        hdr.ad.vuh_atlas_id = vuh_atlas_id;
        break;

    case V3C_OVD:
        hdr.ovd = {};
        hdr.ovd.vuh_v3c_parameter_set_id = vuh_v3c_parameter_set_id;
        hdr.ovd.vuh_atlas_id = vuh_atlas_id;
        break;

    case V3C_GVD:
        hdr.gvd = {};
        hdr.gvd.vuh_v3c_parameter_set_id = vuh_v3c_parameter_set_id;
        hdr.gvd.vuh_atlas_id = vuh_atlas_id;
        // last bit of second byte and 3 first bytes of third byte
        hdr.gvd.vuh_map_index = ((buf[ptr + 1] & 0b1) << 3) | ((buf[ptr + 2] & 0b11100000) >> 5);
        std::cout << "-- vuh_map_index: " << (uint32_t)hdr.gvd.vuh_map_index << std::endl;
        // fourth bit of third byte
        hdr.gvd.vuh_auxiliary_video_flag = (buf[ptr + 2] & 0b00010000) >> 4;
        std::cout << "-- vuh_auxiliary_video_flag: " << (uint32_t)hdr.gvd.vuh_auxiliary_video_flag << std::endl;
        break;

    case V3C_AVD:
        hdr.avd = {};
        hdr.avd.vuh_v3c_parameter_set_id = vuh_v3c_parameter_set_id;
        hdr.avd.vuh_atlas_id = vuh_atlas_id;
        // last bit of second byte and 6 first bytes of third byte
        hdr.avd.vuh_attribute_index = ((buf[ptr + 1] & 0b1) << 6) | ((buf[ptr + 2] & 0b11111100) >> 2);
        std::cout << "-- vuh_attribute_index: " << (uint32_t)hdr.avd.vuh_attribute_index << std::endl;
        // 2 last bits of third byte and 3 first bits of fourth byte
        hdr.avd.vuh_attribute_partition_index = (buf[ptr + 2] & 0b11) | ((buf[ptr + 3] & 0b11100000) >> 5);
        std::cout << "-- vuh_attribute_partition_index: " << (uint32_t)hdr.avd.vuh_attribute_index << std::endl;
        // fourth byte: 4 bits
        hdr.avd.vuh_map_index = (buf[ptr + 3] & 0b00011110) >> 1;
        std::cout << "-- vuh_map_index: " << (uint32_t)hdr.avd.vuh_map_index << std::endl;
        // last bit of fourth byte
        hdr.avd.vuh_auxiliary_video_flag = (buf[ptr + 3] & 0b1);
        std::cout << "-- vuh_auxiliary_video_flag: " << (uint32_t)hdr.avd.vuh_auxiliary_video_flag << std::endl;
        break;

    case V3C_PVD:
        hdr.pvd = {};
        hdr.pvd.vuh_v3c_parameter_set_id = vuh_v3c_parameter_set_id;
        hdr.pvd.vuh_atlas_id = vuh_atlas_id;
        break;

    case V3C_CAD:
        hdr.cad = {};
        hdr.cad.vuh_v3c_parameter_set_id = vuh_v3c_parameter_set_id;
        break;

    default:
        break;
    }
    return;
}

uint64_t get_size(std::string filename)
{
    std::ifstream infile(filename, std::ios_base::binary);
    if (!infile.is_open()) {
        std::cout << "File not found" << std::endl;
        return 0;
    }

    //get length of file
    infile.seekg(0, infile.end);
    size_t length = infile.tellg();
    infile.seekg(0, infile.beg);

    return length;
}

char* get_cmem(std::string filename)
{
    std::ifstream infile(filename, std::ios_base::binary);

    if (!infile.is_open()) {
        std::cout << "File not found" << std::endl;
        return 0;
    }

    //get length of file
    infile.seekg(0, infile.end);
    size_t length = infile.tellg();
    infile.seekg(0, infile.beg);

    char* buf = new char[length];
    // read into char*
    if (!(infile.read(buf, length))) // read up to the size of the buffer
    {
        if (!infile.eof())
        {
            std::cerr << "Failed to read file contents." << std::endl;
            delete[] buf; // Free memory before returning nullptr
            return nullptr;
        }
    }
    return buf;
}

v3c_streams init_v3c_streams(uvgrtp::session* sess, uint16_t src_port, uint16_t dst_port, int flags, bool rec)
{
    flags |= RCE_NO_H26X_PREPEND_SC;
    v3c_streams streams = {};
    /*
    if (rec) {
        streams.vps = sess->create_stream(4998, 4999, RTP_FORMAT_GENERIC, flags);
        streams.ad = sess->create_stream(5000, 5001, RTP_FORMAT_ATLAS, flags);
        streams.ovd = sess->create_stream(5002, 5003, RTP_FORMAT_H265, flags);
        streams.gvd = sess->create_stream(6000, 6002, RTP_FORMAT_H265, flags);
        streams.avd = sess->create_stream(5006, 5007, RTP_FORMAT_H265, flags);
    }
    else {
        streams.vps = sess->create_stream(4999, 4998, RTP_FORMAT_GENERIC, flags);
        streams.ad = sess->create_stream(5001, 5000, RTP_FORMAT_ATLAS, flags);
        streams.ovd = sess->create_stream(5003, 5002, RTP_FORMAT_H265, flags);
        streams.gvd = sess->create_stream(6002, 6000, RTP_FORMAT_H265, flags);
        streams.avd = sess->create_stream(5007, 5006, RTP_FORMAT_H265, flags);
    }*/
    streams.vps = sess->create_stream(src_port, dst_port, RTP_FORMAT_GENERIC, flags);
    streams.ad = sess->create_stream(src_port, dst_port, RTP_FORMAT_ATLAS, flags);
    streams.ovd = sess->create_stream(src_port, dst_port, RTP_FORMAT_H265, flags);
    streams.gvd = sess->create_stream(src_port, dst_port, RTP_FORMAT_H265, flags);
    streams.avd = sess->create_stream(src_port, dst_port, RTP_FORMAT_H265, flags);
    
    if (rec) {
        streams.vps->configure_ctx(RCC_REMOTE_SSRC, 1);
        streams.ad->configure_ctx(RCC_REMOTE_SSRC, 2);
        streams.ovd->configure_ctx(RCC_REMOTE_SSRC, 3);
        streams.gvd->configure_ctx(RCC_REMOTE_SSRC, 4);
        streams.avd->configure_ctx(RCC_REMOTE_SSRC, 5);
    }
    else {
        streams.vps->configure_ctx(RCC_SSRC, 1);
        streams.ad->configure_ctx(RCC_SSRC, 2);
        streams.ovd->configure_ctx(RCC_SSRC, 3);
        streams.gvd->configure_ctx(RCC_SSRC, 4);
        streams.avd->configure_ctx(RCC_SSRC, 5);
    }
    //streams.gvd->configure_ctx(RCC_FPS_NUMERATOR, 10);
    return streams;
}

v3c_file_map init_mmap(uint8_t atlas_nal_precision, uint8_t video_nal_precision)
{
    v3c_file_map mmap = {};

    v3c_unit_header hdr(V3C_AD);
    hdr.ad = { 0, 0 };
    v3c_unit_info unit = {atlas_nal_precision, hdr, {}, 0, false };
    mmap.ad_units.push_back(unit);

    hdr = v3c_unit_header(V3C_OVD);
    hdr.ovd = { 0, 0 };
    unit = {video_nal_precision, hdr, {}, 0, false };
    mmap.ovd_units.push_back(unit);

    hdr = v3c_unit_header(V3C_GVD);
    hdr.gvd = { 0, 0, 0, false};
    unit = {video_nal_precision, hdr, {}, 0, false };
    mmap.gvd_units.push_back(unit);

    hdr = v3c_unit_header(V3C_AVD);
    hdr.avd = { 0, 0, 0, 0, 0, false};
    unit = {video_nal_precision, hdr, {}, 0, false };
    mmap.avd_units.push_back(unit);
    return mmap;
}

void create_v3c_unit(v3c_unit_info& current_unit, char* buf, uint64_t& ptr, uint8_t v3c_precision)
{
    const uint8_t v3c_type = current_unit.header.vuh_unit_type;
    const uint8_t nal_precision = current_unit.sample_stream_nal_precision;

    // V3C unit size
    uint8_t v3c_size_arr[MAX_V3C_SIZE_PREC] = {};
    uint64_t v3c_size_int = get_v3c_unit_size(current_unit, v3c_type);
    
    std::cout << "init v3c unit of size " << v3c_size_int << std::endl;
    convert_size_big_endian(v3c_size_int, v3c_size_arr, v3c_precision);
    memcpy(&buf[ptr], (char *)v3c_size_arr, v3c_precision);
    ptr += v3c_precision;

    // Next up create the V3C unit header
    uint8_t v3c_header[4] = { 0, 0, 0, 0 };

    // All V3C unit types have parameter_set_id in header
    uint8_t param_set_id = current_unit.header.ad.vuh_v3c_parameter_set_id;
    //std::cout << "VUH typ: " << (uint32_t)v3c_type << " param set id " << (uint32_t)param_set_id << std::endl;
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
        uint8_t nal_size_arr[MAX_V3C_SIZE_PREC] = {0};
        convert_size_big_endian(uint32_t(p.size), nal_size_arr, nal_precision);
        memcpy(&buf[ptr], (char *)nal_size_arr, nal_precision);
        ptr += nal_precision;

        // Copy NAL unit
        //memcpy(&buf[ptr], &current_unit.buf[p.location], p.size);
        memcpy(&buf[ptr], p.buf, p.size);
        ptr += p.size;
        delete[] p.buf;
        p.buf = nullptr;
    }

}

uint64_t reconstruct_v3c_gof(bool hdr_byte, char* &buf, uint64_t& ptr, v3c_file_map& mmap, uint64_t index, uint8_t v3c_precision)
{
    uint64_t gof_size = get_gof_size(hdr_byte, index, mmap, v3c_precision);
    std::cout << "Initializing GOF buffer of " << gof_size << " bytes" << std::endl;

    // Commented out because we want to write the whole file into the buffer, not GOF by GOF
    //buf = new char[gof_size];

    // V3C Sample stream header
    if (hdr_byte) {
        uint8_t first_byte = ((v3c_precision - 1) & 0b111) << 5;
        buf[ptr] = first_byte;
        ptr++;
    }

    uint8_t v3c_size_arr[MAX_V3C_SIZE_PREC] = {};

    v3c_unit_info current_unit = mmap.vps_units.at(index); // Now processing VPS unit
    uint32_t v3c_size_int = (uint32_t)current_unit.nal_infos.at(0).size;

    // Write the V3C VPS unit size to the output buffer
    convert_size_big_endian(v3c_size_int, v3c_size_arr, v3c_precision);
    memcpy(&buf[ptr], (char *)v3c_size_arr, v3c_precision);
    ptr += v3c_precision;

    // Write the V3C VPS unit payload to the output buffer
    //memcpy(&buf[ptr], current_unit.buf, v3c_size_int);
    memcpy(&buf[ptr], current_unit.nal_infos.back().buf, v3c_size_int);
    ptr += v3c_size_int;

    // Write out V3C AD unit
    current_unit = mmap.ad_units.at(index);
    create_v3c_unit(current_unit, buf, ptr, v3c_precision);

    // Write out V3C OVD unit
    current_unit = mmap.ovd_units.at(index);
    create_v3c_unit(current_unit, buf, ptr, v3c_precision);

    // Write out V3C GVD unit
    current_unit = mmap.gvd_units.at(index);
    create_v3c_unit(current_unit, buf, ptr, v3c_precision);

    // Write out V3C AVD unit
    current_unit = mmap.avd_units.at(index);
    create_v3c_unit(current_unit, buf, ptr, v3c_precision);
    
    return gof_size;
}

bool is_gof_ready(uint64_t index, v3c_file_map& mmap)
{
    if (mmap.vps_units.size() < index+1) 
        return false;
    if (mmap.ad_units.size() < index+1 || !mmap.ad_units.at(index).ready)
        return false;
    if (mmap.ovd_units.size() < index+1 || !mmap.ovd_units.at(index).ready)
        return false;
    if (mmap.gvd_units.size() < index+1 || !mmap.gvd_units.at(index).ready)
        return false;
    if (mmap.avd_units.size() < index+1 || !mmap.avd_units.at(index).ready)
        return false;
    return true;
}

void copy_rtp_payload(std::vector<v3c_unit_info>* units, uint64_t max_size, uvgrtp::frame::rtp_frame* frame)
{
    uint32_t seq = frame->header.seq;
    if (units->back().nal_infos.size() == max_size) {
        v3c_unit_header hdr(units->back().header.vuh_unit_type);
        v3c_unit_info info = {0, hdr, {}, 0, false };
        switch (units->back().header.vuh_unit_type) {
            case V3C_AD: {
                info.sample_stream_nal_precision = DEFAULT_ATLAS_NAL_SIZE_PRECISION;
                info.header.ad = { (uint8_t)units->size(), 0};
                break;
            }
            case V3C_OVD: {
                info.sample_stream_nal_precision = DEFAULT_VIDEO_NAL_SIZE_PRECISION;
                info.header.ovd = { (uint8_t)units->size(), 0 };
                break;
            }
            case V3C_GVD: {
                info.sample_stream_nal_precision = DEFAULT_VIDEO_NAL_SIZE_PRECISION;
                info.header.gvd = { (uint8_t)units->size(), 0, 0, 0 };
                break;
            }
            case V3C_AVD: {
                info.sample_stream_nal_precision = DEFAULT_VIDEO_NAL_SIZE_PRECISION;
                info.header.avd = { (uint8_t)units->size(), 0 };
                break;
            }
        }
        units->push_back(info);
    }

    if (units->back().nal_infos.size() <= max_size) {
        char* cbuf = new char[frame->payload_len];
        memcpy(cbuf, frame->payload, frame->payload_len);
        nal_info nalu = { units->back().ptr, frame->payload_len, cbuf};
        units->back().nal_infos.push_back(nalu);
        units->back().ptr += frame->payload_len;
    }
    if (units->back().nal_infos.size() == max_size) {
        units->back().ready = true;
    }
}


void finalize_gof(v3c_file_map& mmap)
{
  mmap.ad_units.back().ready = true;
  mmap.ovd_units.back().ready = true;
  mmap.gvd_units.back().ready = true;
  mmap.avd_units.back().ready = true;
}

uint64_t get_gof_size(bool hdr_byte, uint64_t index, v3c_file_map& mmap, uint8_t v3c_precision)
{
    /* Calculate GOF size and intialize the output buffer
    *
    + 1 byte of Sample Stream Precision
    +----------------------------------------------------------------+
    + V3C_SIZE_PRECISION bytes of V3C Unit size
    + x1 bytes of whole V3C VPS unit (incl. header)
    +----------------------------------------------------------------+
      Atlas V3C unit
    + V3C_SIZE_PRECISION bytes for V3C Unit size
    + 4 bytes of V3C header
    + 1 byte of NAL Unit Size Precision (x1)
    + NALs count (x1 bytes of NAL Unit Size
    + x2 bytes of NAL unit payload)
    +----------------------------------------------------------------+
      Video V3C unit
    + V3C_SIZE_PRECISION bytes for V3C Unit size
    + 4 bytes of V3C header
    + NALs count (4 bytes of NAL Unit Size
    + x2 bytes of NAL unit payload)
    +----------------------------------------------------------------+
                .
                .
                .
    +----------------------------------------------------------------+
      Video V3C unit
    + V3C_SIZE_PRECISION bytes for V3C Unit size
    + 4 bytes of V3C header
    + NALs count (4 bytes of NAL Unit Size
    + x2 bytes of NAL unit payload)
    +----------------------------------------------------------------+ */
    uint64_t gof_size = 0;
    if (hdr_byte) {
        gof_size++; // Sample Stream Precision
    }

    // These sizes include the V3C unit size field, header and payload
    uint64_t vps_size = get_v3c_unit_size<V3C_VPS>(mmap.vps_units.at(index)) + v3c_precision;
    uint64_t  ad_size = get_v3c_unit_size<V3C_AD>(mmap.ad_units.at(index))   + v3c_precision;
    uint64_t ovd_size = get_v3c_unit_size<V3C_OVD>(mmap.ovd_units.at(index)) + v3c_precision;
    uint64_t gvd_size = get_v3c_unit_size<V3C_GVD>(mmap.gvd_units.at(index)) + v3c_precision;
    uint64_t avd_size = get_v3c_unit_size<V3C_AVD>(mmap.avd_units.at(index)) + v3c_precision;
    gof_size += vps_size + ad_size + ovd_size + gvd_size + avd_size;
    return gof_size;
}

template <V3C_UNIT_TYPE E>
uint64_t get_v3c_unit_size(const v3c_unit_info &info)
{
    std::cout << "Error: Unit type not supported" << std::endl;
    return 0;
}

template <>
uint64_t get_v3c_unit_size<V3C_VPS>(const v3c_unit_info &info)
{
    return                                                + info.nal_infos.at(0).size; // incl. header
}
template <>
uint64_t get_v3c_unit_size<V3C_AD>(const v3c_unit_info &info)
{
    return V3C_HDR_LEN + SAMPLE_STREAM_HDR_LEN + info.ptr + info.nal_infos.size() * info.sample_stream_nal_precision;

}

template <>
uint64_t get_v3c_unit_size<V3C_OVD>(const v3c_unit_info &info)
{
    return V3C_HDR_LEN +                         info.ptr + info.nal_infos.size() * info.sample_stream_nal_precision;
}

template <>
uint64_t get_v3c_unit_size<V3C_GVD>(const v3c_unit_info &info)
{
    return V3C_HDR_LEN +                         info.ptr + info.nal_infos.size() * info.sample_stream_nal_precision;
}

template <>
uint64_t get_v3c_unit_size<V3C_AVD>(const v3c_unit_info &info)
{
    return V3C_HDR_LEN +                         info.ptr + info.nal_infos.size() * info.sample_stream_nal_precision;
}

uint64_t get_v3c_unit_size(const v3c_unit_info &info, const uint8_t unit_type)
{
    switch (unit_type)
    {
    case V3C_VPS:
        return get_v3c_unit_size<V3C_VPS>(info);
    
    case V3C_AD:
        return get_v3c_unit_size<V3C_AD>(info);
    
    case V3C_OVD:
        return get_v3c_unit_size<V3C_OVD>(info);
    
    case V3C_GVD:
        return get_v3c_unit_size<V3C_GVD>(info);

    case V3C_AVD:
        return get_v3c_unit_size<V3C_AVD>(info);
    
    default:
        return get_v3c_unit_size<V3C_UNDEF>(info);
    }
}