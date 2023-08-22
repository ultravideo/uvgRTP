#include "uvgrtp/v3c_parser.hh"

uint32_t combineBytes(uint8_t byte1, uint8_t byte2, uint8_t byte3, uint8_t byte4) {
    return (static_cast<uint32_t>(byte1) << 24) |
        (static_cast<uint32_t>(byte2) << 16) |
        (static_cast<uint32_t>(byte3) << 8) |
        static_cast<uint32_t>(byte4);
}

uint32_t combineBytes(uint8_t byte1, uint8_t byte2, uint8_t byte3) {
    return (static_cast<uint32_t>(byte1) << 16) |
        (static_cast<uint32_t>(byte2) << 8) |
        static_cast<uint32_t>(byte3);
}

uint32_t combineBytes(uint8_t byte1, uint8_t byte2) {
    return (static_cast<uint32_t>(byte1) << 8) |
        (static_cast<uint32_t>(byte2));
}

void convert_size_little_endian(uint32_t in, uint8_t* out, size_t output_size) {
    // Make sure the output size is not larger than the size of uint32_t
    if (output_size > sizeof(uint32_t)) {
        output_size = sizeof(uint32_t);
    }

    // Convert the uint32_t input into the output array
    // The exact way to store the value depends on the endianness of the system
    // This example assumes little-endian
    for (size_t i = 0; i < output_size; ++i) {
        out[i] = (in >> (8 * i)) & 0xFF;
    }
}

void convert_size_big_endian(uint32_t in, uint8_t* out, size_t output_size) {
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
    std::cout << "V3C size precision: " << (uint32_t)v3c_size_precision << std::endl;
    std::cout << std::endl;
    ++ptr;

    uint8_t* v3c_size = new uint8_t[v3c_size_precision];

    uint8_t nal_size_precision = 0;
    while (true) {
        if (ptr >= len) {
            break;
        }
        // Readthe V3C unit size (2-3 bytes usually)
        memcpy(v3c_size, &cbuf[ptr], v3c_size_precision);
        ptr += v3c_size_precision; // Jump over the V3C unit size bytes
        uint32_t combined_v3c_size = 0;
        if (v3c_size_precision == 2) {
            combined_v3c_size = combineBytes(v3c_size[0], v3c_size[1]);
        }
        else if (v3c_size_precision == 3) {
            combined_v3c_size = combineBytes(v3c_size[0], v3c_size[1], v3c_size[2]);
        }
        else {
            std::cout << "Error " << std::endl;
            return EXIT_FAILURE;
        }
        // Inside v3c unit now
        std::cout << "Current V3C unit location " << ptr << ", size " << combined_v3c_size << std::endl;
        uint64_t v3c_ptr = ptr;

        // Next 4 bytes are the V3C unit header
        v3c_unit_header v3c_hdr = {};
        parse_v3c_header(v3c_hdr, cbuf, v3c_ptr);
        uint8_t vuh_t = v3c_hdr.vuh_unit_type;
        std::cout << "-- vuh_unit_type: " << (uint32_t)vuh_t << std::endl;
        v3c_unit_info unit = { v3c_hdr, {}, nullptr};

        if (vuh_t == V3C_VPS) {
            // Parameter set contains no NAL units, skip over
            std::cout << "-- Parameter set V3C unit" << std::endl;
            unit.nal_infos.push_back({ ptr, combined_v3c_size });
            mmap.vps_units.push_back(unit);
            ptr += combined_v3c_size;
            std::cout << std::endl;
            continue;
        }

        // Rest of the function goes inside the V3C unit payload and parses it into NAL units
        v3c_ptr += V3C_HDR_LEN; // Jump over 4 bytes of V3C unit header
        if (vuh_t == V3C_AD || vuh_t == V3C_CAD) {
            uint8_t v3cu_first_byte = cbuf[v3c_ptr]; // Next up is 1 byte of NAL unit size precision
            nal_size_precision = (v3cu_first_byte >> 5) + 1;
            std::cout << "  -- Atlas NAL Sample stream, 1 byte for NAL unit size precision: " << (uint32_t)nal_size_precision << std::endl;
            ++v3c_ptr;
        }
        else {
            nal_size_precision = 4;
            std::cout << "  -- Video NAL Sample stream, using NAL unit size precision of: " << (uint32_t)nal_size_precision << std::endl;
        }
        uint64_t amount_of_nal_units = 0;
        // Now start to parse the NAL sample stream
        while (true) {
            if (v3c_ptr >= (ptr + combined_v3c_size)) {
                break;
            }
            amount_of_nal_units++;
            uint32_t combined_nal_size = 0;
            if (nal_size_precision == 2) {
                combined_nal_size = combineBytes(cbuf[v3c_ptr], cbuf[v3c_ptr + 1]);
            }
            else if (nal_size_precision == 3) {
                combined_nal_size = combineBytes(cbuf[v3c_ptr], cbuf[v3c_ptr + 1], cbuf[v3c_ptr + 2]);
            }
            else if (nal_size_precision == 4) {
                combined_nal_size = combineBytes(cbuf[v3c_ptr], cbuf[v3c_ptr + 1], cbuf[v3c_ptr + 2], cbuf[v3c_ptr + 3]);
            }
            else {
                std::cout << "  -- Error, invalid NAL size " << std::endl;
                return EXIT_FAILURE;
            }
            v3c_ptr += nal_size_precision;
            switch (vuh_t) {
            case V3C_AD:
            case V3C_CAD:
                std::cout << "  -- v3c_ptr: " << v3c_ptr << ", NALU size: " << combined_nal_size << std::endl;
                break;
            case V3C_OVD:
            case V3C_GVD:
            case V3C_AVD:
            case V3C_PVD:
                uint8_t h265_nalu_t = (cbuf[v3c_ptr] & 0b01111110) >> 1;
                std::cout << "  -- v3c_ptr: " << v3c_ptr << ", NALU size: " << combined_nal_size << ", HEVC NALU type: " << (uint32_t)h265_nalu_t << std::endl;
            }
            unit.nal_infos.push_back({ v3c_ptr, combined_nal_size });
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
        // 2 last bits of third byte and 3 first b�ts of fourth byte
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

void parse_vps_ptl(profile_tier_level &ptl, char* buf, uint64_t ptr)
{
    // first bit of first byte
    ptl.ptl_tier_flag = buf[ptr + 4] >> 7;
    std::cout << "-- ptl_tier_flag: " << (uint32_t)ptl.ptl_tier_flag << std::endl;
    // 7 bits after
    ptl.ptl_profile_codec_group_idc = buf[ptr + 4] & 0b01111111;
    std::cout << "-- ptl_profile_codec_group_idc: " << (uint32_t)ptl.ptl_profile_codec_group_idc << std::endl;
    // 8 bits after
    ptl.ptl_profile_toolset_idc = buf[ptr + 5];
    std::cout << "-- ptl_profile_toolset_idc: " << (uint32_t)ptl.ptl_profile_toolset_idc << std::endl;
    // 8 bits after
    ptl.ptl_profile_reconstruction_idc = buf[ptr + 6];
    std::cout << "-- ptl_profile_reconstruction_idc: " << (uint32_t)ptl.ptl_profile_reconstruction_idc << std::endl;
    // 16 reserved bits
    //4 bits after
    ptl.ptl_max_decodes_idc = buf[ptr + 9] >> 4;
    std::cout << "-- ptl_max_decodes_idc: " << (uint32_t)ptl.ptl_max_decodes_idc << "+1 = " <<
        (uint32_t)ptl.ptl_max_decodes_idc + 1 << std::endl;
    // 12 reserved bits
    // 8 bits after
    ptl.ptl_level_idc = buf[ptr + 11];
    std::cout << "-- ptl_level_idc: " << (uint32_t)ptl.ptl_level_idc << "/30 = " <<
        (double)ptl.ptl_level_idc / 30 << std::endl;
    // 6 bits after
    ptl.ptl_num_sub_profiles = buf[ptr + 12] >> 2;
    std::cout << "-- ptl_num_sub_profiles: " << (uint32_t)ptl.ptl_num_sub_profiles << std::endl;
    // 1 bit after
    ptl.ptl_extended_sub_profile_flag = (buf[ptr + 12] & 0b10) >> 1;
    std::cout << "-- ptl_extended_sub_profile_flag: " << (uint32_t)ptl.ptl_extended_sub_profile_flag << std::endl;
    // next up are the sub-profile IDs. They can be either 32 or 64 bits long, indicated by ptl_extended_sub_profile_flag
    // Note: This has not been tested. But it should work
    ptr += 12;
    uint64_t first_full_byte = ptr + 13;
    if (ptl.ptl_extended_sub_profile_flag == 1) {
        for (int i = 0; i < ptl.ptl_num_sub_profiles; i++) {
            // TODO this isnt right...
            uint64_t sub_profile_id = (buf[first_full_byte] >> 1) | ((buf[first_full_byte - 1] & 0b1) << 63);
            ptl.ptl_sub_profile_idc.push_back(sub_profile_id);
            first_full_byte += 8;
        }
    }
    else {
        for (int i = 0; i < ptl.ptl_num_sub_profiles; i++) {
            uint32_t sub_profile_id = (buf[first_full_byte] >> 1) | ((buf[first_full_byte - 1] & 0b1) << 31);
            ptl.ptl_sub_profile_idc.push_back((uint64_t)sub_profile_id);
            first_full_byte += 4;
        }
    }
    // 1 bit after
    ptl.ptl_toolset_constraints_present_flag = (buf[ptr] & 0b1);
    std::cout << "-- ptl_toolset_constraints_present_flag: " << (uint32_t)ptl.ptl_toolset_constraints_present_flag << std::endl;
}

uint64_t get_size(std::string filename)
{
    std::ifstream infile(filename, std::ios_base::binary);


    //get length of file
    infile.seekg(0, infile.end);
    size_t length = infile.tellg();
    infile.seekg(0, infile.beg);

    return length;
}

char* get_cmem(std::string filename, const size_t& len)
{
    std::ifstream infile(filename, std::ios_base::binary);

    char* buf = new char[len];
    // read into char*
    if (!(infile.read(buf, len))) // read up to the size of the buffer
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