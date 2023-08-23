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

uint64_t get_size(std::string filename)
{
    std::ifstream infile(filename, std::ios_base::binary);

    //get length of file
    infile.seekg(0, infile.end);
    size_t length = infile.tellg();
    infile.seekg(0, infile.beg);

    return length;
}

char* get_cmem(std::string filename)
{
    std::ifstream infile(filename, std::ios_base::binary);

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
    return streams;
}

v3c_file_map init_mmap()
{
    v3c_file_map mmap = {};

    v3c_unit_header hdr = { V3C_AD };
    hdr.ad = { 0, 0 };
    v3c_unit_info unit = { hdr, {}, new char[INPUT_BUFFER_SIZE], 0, false };
    mmap.ad_units.push_back(unit);

    hdr = { V3C_OVD };
    hdr.ovd = { 0, 0 };
    unit = { hdr, {}, new char[INPUT_BUFFER_SIZE], 0, false };
    mmap.ovd_units.push_back(unit);

    hdr = { V3C_GVD };
    hdr.gvd = { 0, 0 };
    unit = { hdr, {}, new char[INPUT_BUFFER_SIZE], 0, false };
    mmap.gvd_units.push_back(unit);

    hdr = { V3C_AVD };
    hdr.avd = { 0, 0 };
    unit = { hdr, {}, new char[INPUT_BUFFER_SIZE], 0, false };
    mmap.avd_units.push_back(unit);
    return mmap;
}

void create_v3c_unit(v3c_unit_info& current_unit, char* buf, uint64_t& ptr, uint64_t v3c_precision, uint32_t nal_precision)
{
    uint8_t v3c_type = current_unit.header.vuh_unit_type;

    // V3C unit size
    uint8_t* v3c_size_arr = new uint8_t[v3c_precision];
    uint32_t v3c_size_int = 4 + (uint32_t)current_unit.ptr + (uint32_t)current_unit.nal_infos.size() * nal_precision;
    if (v3c_type == V3C_AD || v3c_type == V3C_CAD) {
        v3c_size_int++; // NAL size precision for Atlas V3C units
    }
    convert_size_big_endian(v3c_size_int, v3c_size_arr, v3c_precision);
    memcpy(&buf[ptr], v3c_size_arr, v3c_precision);
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
        uint8_t* nal_size_arr = new uint8_t[nal_precision];
        convert_size_big_endian(uint32_t(p.size), nal_size_arr, nal_precision);
        memcpy(&buf[ptr], nal_size_arr, nal_precision);
        ptr += nal_precision;

        // Copy NAL unit
        memcpy(&buf[ptr], &current_unit.buf[p.location], p.size);
        ptr += p.size;
    }

}

uint64_t reconstruct_v3c_gop(bool hdr_byte, char* &buf, uint64_t& ptr, v3c_file_map& mmap, uint64_t index)
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
    uint64_t gop_size = 0;
    /*if (hdr_byte) {
        //gop_size++; // Sample Stream Precision
    }*/
    uint64_t vps_size = mmap.vps_units.at(index).nal_infos.at(0).size; // incl. header
    uint64_t ad_size = 4 + 4 + 1 + mmap.ad_units.at(index).ptr + mmap.ad_units.at(index).nal_infos.size() * ATLAS_NAL_SIZE_PRECISION;
    uint64_t ovd_size = 8 + mmap.ovd_units.at(index).ptr + mmap.ovd_units.at(index).nal_infos.size() * VIDEO_NAL_SIZE_PRECISION;
    uint64_t gvd_size = 8 + mmap.gvd_units.at(index).ptr + mmap.gvd_units.at(index).nal_infos.size() * VIDEO_NAL_SIZE_PRECISION;
    uint64_t avd_size = 8 + mmap.avd_units.at(index).ptr + mmap.avd_units.at(index).nal_infos.size() * VIDEO_NAL_SIZE_PRECISION;
    gop_size += vps_size + ad_size + ovd_size + gvd_size + avd_size;
    std::cout << "Initializing GoP buffer of " << gop_size << " bytes" << std::endl;

    buf = new char[gop_size];

    // V3C Sample stream header
    if (hdr_byte) {
        uint8_t first_byte = 64;
        buf[ptr] = first_byte;
        ptr++;
    }

    uint8_t* v3c_size_arr = new uint8_t[V3C_SIZE_PRECISION];

    v3c_unit_info current_unit = mmap.vps_units.at(index); // Now processing VPS unit
    uint32_t v3c_size_int = (uint32_t)current_unit.nal_infos.at(0).size;

    // Write the V3C VPS unit size to the output buffer
    convert_size_big_endian(v3c_size_int, v3c_size_arr, V3C_SIZE_PRECISION);
    memcpy(&buf[ptr], v3c_size_arr, V3C_SIZE_PRECISION);
    ptr += V3C_SIZE_PRECISION;

    // Write the V3C VPS unit payload to the output buffer
    memcpy(&buf[ptr], current_unit.buf, v3c_size_int);
    ptr += v3c_size_int;

    // Write out V3C AD unit
    current_unit = mmap.ad_units.at(index);
    create_v3c_unit(current_unit, buf, ptr, V3C_SIZE_PRECISION, ATLAS_NAL_SIZE_PRECISION);

    // Write out V3C OVD unit
    current_unit = mmap.ovd_units.at(index);
    create_v3c_unit(current_unit, buf, ptr, V3C_SIZE_PRECISION, VIDEO_NAL_SIZE_PRECISION);

    // Write out V3C GVD unit
    current_unit = mmap.gvd_units.at(index);
    create_v3c_unit(current_unit, buf, ptr, V3C_SIZE_PRECISION, VIDEO_NAL_SIZE_PRECISION);

    // Write out V3C AVD unit
    current_unit = mmap.avd_units.at(index);
    create_v3c_unit(current_unit, buf, ptr, V3C_SIZE_PRECISION, VIDEO_NAL_SIZE_PRECISION);

    return gop_size;
}

bool is_gop_ready(uint64_t index, v3c_file_map& mmap)
{
    if (mmap.vps_units.size() < index)
        return false;
    if (mmap.ad_units.size() < index || !mmap.ad_units.at(index - 1).ready)
        return false;
    if (mmap.ovd_units.size() < index || !mmap.ovd_units.at(index - 1).ready)
        return false;
    if (mmap.gvd_units.size() < index || !mmap.gvd_units.at(index - 1).ready)
        return false;
    if (mmap.avd_units.size() < index || !mmap.avd_units.at(index - 1).ready)
        return false;

    return true;
}

void copy_rtp_payload(std::vector<v3c_unit_info>& units, uint64_t max_size, uvgrtp::frame::rtp_frame* frame)
{
    if ((units.end() - 1)->nal_infos.size() == max_size) {
        v3c_unit_header hdr = { (units.end() - 1)->header.vuh_unit_type };
        v3c_unit_info info = { {}, {}, new char[INPUT_BUFFER_SIZE], 0, false };

        switch ((units.end() - 1)->header.vuh_unit_type) {
            case V3C_AD: {
                info.header.ad = { (uint8_t)units.size(), 0 };
                break;
            }
            case V3C_OVD: {
                info.header.ovd = { (uint8_t)units.size(), 0 };
                break;
            }
            case V3C_GVD: {
                info.header.gvd = { (uint8_t)units.size(), 0, 0, 0 };
                break;
            }
            case V3C_AVD: {
                info.header.avd = { (uint8_t)units.size(), 0 };
                break;
            }
        }
        units.push_back(info);
    }
    auto current = units.end() - 1;

    if (current->nal_infos.size() <= max_size) {
        memcpy(&current->buf[current->ptr], frame->payload, frame->payload_len);
        current->nal_infos.push_back({ current->ptr, frame->payload_len });
        current->ptr += frame->payload_len;
    }
    if (current->nal_infos.size() == max_size) {
        current->ready = true;
    }
}