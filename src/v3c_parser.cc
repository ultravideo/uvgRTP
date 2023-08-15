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

bool mmap_v3c_file(char* cbuf, uint64_t len, vuh_vps& param, std::vector<std::pair<uint64_t, uint64_t>>& ad,
    std::vector<std::pair<uint64_t, uint64_t>>& vd)
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

    std::vector<uint32_t> sizes = {};
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
        std::cout << "Current V3C unit size " << combined_v3c_size << std::endl;
        uint64_t v3c_ptr = ptr;

        // Next 4 bytes are the V3C unit header
        v3c_unit_header v3c_hdr = {};
        parse_v3c_header(v3c_hdr, cbuf, v3c_ptr);
        uint8_t vuh_t = v3c_hdr.vuh_unit_type;
        std::cout << "-- vuh_unit_type: " << (uint32_t)vuh_t << std::endl;

        if (vuh_t == V3C_VPS) {
            // Parameter set contains no NAL units, skip over
            std::cout << "-- Parameter set V3C unit" << std::endl;
            ptr += combined_v3c_size;
            sizes.push_back(combined_v3c_size);
            param = v3c_hdr.vps;
            std::cout << std::endl;
            continue;
        }
        else if (vuh_t == V3C_OVD || vuh_t == V3C_GVD || vuh_t == V3C_AVD || vuh_t == V3C_PVD) {
            // Video data, map start and size
            std::cout << "-- Video data V3C unit, " << std::endl;
            vd.push_back({ ptr + V3C_HDR_LEN, combined_v3c_size });
            ptr += combined_v3c_size;
            std::cout << std::endl;
            continue;
        }

        // Rest of the function goes inside the V3C unit payload and parses NAL it into NAL units
        v3c_ptr += V3C_HDR_LEN; // Jump over 4 bytes of V3C unit header

        uint8_t v3cu_first_byte = cbuf[v3c_ptr]; // Next up is 1 byte of NAL unit size precision
        uint8_t nal_size_precision = (v3cu_first_byte >> 5) + 1;
        std::cout << "  -- NAL Sample stream, 1 byte for NAL unit size precision: " << (uint32_t)nal_size_precision << std::endl;
        ++v3c_ptr;

        // Now start to parse the NAL sample stream
        while (true) {
            if (v3c_ptr >= (ptr + combined_v3c_size)) {
                break;
            }
            uint32_t combined_nal_size = 0;
            if (nal_size_precision == 2) {
                combined_nal_size = combineBytes(cbuf[v3c_ptr], cbuf[v3c_ptr + 1]);
            }
            else if (nal_size_precision == 3) {
                combined_nal_size = combineBytes(cbuf[v3c_ptr], cbuf[v3c_ptr + 1], cbuf[v3c_ptr + 2]);
            }
            else {
                std::cout << "  -- Error, invalid NAL size " << std::endl;
                return EXIT_FAILURE;
            }
            v3c_ptr += nal_size_precision;
            std::cout << "  -- NALU size: " << combined_nal_size << std::endl;
            switch (vuh_t) {
            case V3C_AD:
            case V3C_CAD:
                ad.push_back({ v3c_ptr, combined_nal_size });
                break;

            }
            v3c_ptr += combined_nal_size;
        }



        std::cout << std::endl;
        ptr += combined_v3c_size;
        sizes.push_back(combined_v3c_size);
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
    case V3C_VPS:
        hdr.vps = {};
        // first bit of first byte
        hdr.vps.ptl.ptl_tier_flag = buf[ptr + 4] >> 7;
        std::cout << "-- ptl_tier_flag: " << (uint32_t)hdr.vps.ptl.ptl_tier_flag << std::endl;
        // 7 bits after
        hdr.vps.ptl.ptl_profile_codec_group_idc = buf[ptr + 4] & 0b01111111;
        std::cout << "-- ptl_profile_codec_group_idc: " << (uint32_t)hdr.vps.ptl.ptl_profile_codec_group_idc << std::endl;
        break;

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