#include "test_common.hh"

#include "rtp.hh"
#include "uvgrtp/frame.hh"

#include <cstring>
#include <memory>
#include <random>
#include <vector>

static std::shared_ptr<std::atomic<uint32_t>> make_ssrc()
{
    return std::make_shared<std::atomic<uint32_t>>(0x12345678);
}

static void check_no_crash(uvgrtp::rtp& rtp_inst, const std::vector<uint8_t>& pkt)
{
    uvgrtp::frame::rtp_frame* out = nullptr;
    rtp_error_t ret = rtp_inst.packet_handler(nullptr, 0,
        const_cast<uint8_t*>(pkt.data()), pkt.size(), &out);

    if (ret == RTP_PKT_MODIFIED && out != nullptr) {
        (void)uvgrtp::frame::dealloc_frame(out);
    }
}

TEST(RTPFuzzer, random_bytes_varying_lengths)
{
    uvgrtp::rtp rtp_inst(RTP_FORMAT_GENERIC, make_ssrc(), false);
    std::mt19937 rng(0xDEADBEEF);

    std::vector<size_t> lengths = {0, 1, 4, 8, 11, 12, 13, 16, 20, 50, 100, 500, 1464};
    for (size_t len : lengths) {
        for (int trial = 0; trial < 100; ++trial) {
            std::vector<uint8_t> pkt(len);
            for (auto& b : pkt) b = static_cast<uint8_t>(rng());
            check_no_crash(rtp_inst, pkt);
        }
    }
}

TEST(RTPFuzzer, valid_header_random_rest)
{
    uvgrtp::rtp rtp_inst(RTP_FORMAT_GENERIC, make_ssrc(), false);
    std::mt19937 rng(0xCAFEBABE);

    for (int trial = 0; trial < 500; ++trial) {
        size_t len = 12 + (rng() % 1500);
        std::vector<uint8_t> pkt(len);
        for (auto& b : pkt) b = static_cast<uint8_t>(rng());

        pkt[0] = (2 << 6) | (rng() & 0x3F);
        pkt[1] = static_cast<uint8_t>(rng());

        check_no_crash(rtp_inst, pkt);
    }
}

TEST(RTPFuzzer, valid_header_random_ext_length)
{
    uvgrtp::rtp rtp_inst(RTP_FORMAT_GENERIC, make_ssrc(), false);
    std::mt19937 rng(0xBAADF00D);

    for (int trial = 0; trial < 500; ++trial) {
        size_t len = 12 + (rng() % 500);
        std::vector<uint8_t> pkt(len, 0);

        pkt[0] = (2 << 6) | (1 << 4) | (rng() & 0x0F);
        pkt[1] = static_cast<uint8_t>(rng());
        pkt[8] = 0xAA; pkt[9] = 0xBB; pkt[10] = 0xCC; pkt[11] = 0xDD;

        if (len >= 16) {
            pkt[12] = static_cast<uint8_t>(rng());
            pkt[13] = static_cast<uint8_t>(rng());
            pkt[14] = static_cast<uint8_t>(rng());
            pkt[15] = static_cast<uint8_t>(rng());
        }

        for (size_t i = 16; i < len; ++i)
            pkt[i] = static_cast<uint8_t>(rng());

        check_no_crash(rtp_inst, pkt);
    }
}

TEST(RTPFuzzer, structured_header_all_field_combos)
{
    uvgrtp::rtp rtp_inst(RTP_FORMAT_GENERIC, make_ssrc(), false);
    std::mt19937 rng(0x12345678);

    for (int trial = 0; trial < 1000; ++trial) {
        uint8_t version = (rng() % 4);
        bool has_padding = (rng() % 2) != 0;
        bool has_ext = (rng() % 2) != 0;
        uint8_t cc = rng() & 0x0F;

        size_t hdr_len = 12 + cc * 4;
        size_t ext_len = 0;
        if (has_ext) ext_len = 4 + (rng() % 20) * 4;
        size_t payload_len = rng() % 200;
        size_t pad_len = has_padding ? (1 + rng() % 20) : 0;
        size_t total = hdr_len + ext_len + payload_len + pad_len;
        if (total > 2000) total = 2000;

        std::vector<uint8_t> pkt(total, 0);
        pkt[0] = (version << 6) | (has_padding ? (1 << 5) : 0)
               | (has_ext ? (1 << 4) : 0) | (cc & 0x0F);
        pkt[1] = static_cast<uint8_t>(rng());
        pkt[2] = static_cast<uint8_t>(rng());
        pkt[3] = static_cast<uint8_t>(rng());
        pkt[4] = static_cast<uint8_t>(rng());
        pkt[5] = static_cast<uint8_t>(rng());
        pkt[6] = static_cast<uint8_t>(rng());
        pkt[7] = static_cast<uint8_t>(rng());
        pkt[8] = static_cast<uint8_t>(rng());
        pkt[9] = static_cast<uint8_t>(rng());
        pkt[10] = static_cast<uint8_t>(rng());
        pkt[11] = static_cast<uint8_t>(rng());

        size_t offset = 12;
        for (uint8_t i = 0; i < cc && offset + 4 <= total; ++i) {
            pkt[offset++] = static_cast<uint8_t>(rng());
            pkt[offset++] = static_cast<uint8_t>(rng());
            pkt[offset++] = static_cast<uint8_t>(rng());
            pkt[offset++] = static_cast<uint8_t>(rng());
        }

        if (has_ext && offset + 4 <= total) {
            pkt[offset++] = static_cast<uint8_t>(rng());
            pkt[offset++] = static_cast<uint8_t>(rng());
            pkt[offset++] = static_cast<uint8_t>(rng());
            pkt[offset++] = static_cast<uint8_t>(rng());
        }

        for (size_t i = offset; i < total; ++i)
            pkt[i] = static_cast<uint8_t>(rng());

        if (has_padding && total > 0) {
            pkt[total - 1] = static_cast<uint8_t>(1 + rng() % 20);
        }

        check_no_crash(rtp_inst, pkt);
    }
}

TEST(RTPFuzzer, poc_mutation)
{
    uvgrtp::rtp rtp_inst(RTP_FORMAT_GENERIC, make_ssrc(), false);
    std::mt19937 rng(0xFEEDFACE);

    // 90 = version=2, ext=1, cc=0; 00 00 FF FF = ext len=65535 words
    std::vector<uint8_t> base_pkt = {
        0x90, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00,
        0x12, 0x34, 0x56, 0x78,
        0x00, 0x00, 0xFF, 0xFF
    };

    for (int trial = 0; trial < 500; ++trial) {
        std::vector<uint8_t> pkt = base_pkt;

        int num_mutations = 1 + (rng() % 5);
        for (int m = 0; m < num_mutations; ++m) {
            size_t idx = rng() % pkt.size();
            pkt[idx] = static_cast<uint8_t>(rng());
        }

        if (rng() % 3 == 0) {
            size_t new_len = rng() % 200;
            pkt.resize(new_len);
            for (size_t i = base_pkt.size(); i < new_len; ++i)
                pkt[i] = static_cast<uint8_t>(rng());
        }

        check_no_crash(rtp_inst, pkt);
    }
}

TEST(RTPFuzzer, ext_bit_set_random_csrc_and_ext)
{
    uvgrtp::rtp rtp_inst(RTP_FORMAT_GENERIC, make_ssrc(), false);
    std::mt19937 rng(0xDEADDEAD);

    for (int trial = 0; trial < 500; ++trial) {
        uint8_t cc = rng() & 0x0F;
        size_t hdr_len = 12 + cc * 4;
        size_t ext_hdr = 4;
        size_t ext_data_len = (rng() % 100) * 4;
        size_t payload_len = rng() % 100;
        size_t total = hdr_len + ext_hdr + ext_data_len + payload_len;
        if (total > 2000) total = hdr_len + ext_hdr;

        std::vector<uint8_t> pkt(total);
        for (auto& b : pkt) b = static_cast<uint8_t>(rng());

        pkt[0] = (2 << 6) | (1 << 4) | (cc & 0x0F);
        pkt[8] = 0x11; pkt[9] = 0x22; pkt[10] = 0x33; pkt[11] = 0x44;

        if (total >= hdr_len + 4) {
            size_t off = hdr_len;
            pkt[off + 0] = static_cast<uint8_t>(rng());
            pkt[off + 1] = static_cast<uint8_t>(rng());
            uint16_t claimed_words = static_cast<uint16_t>(rng());
            pkt[off + 2] = (claimed_words >> 8) & 0xFF;
            pkt[off + 3] = claimed_words & 0xFF;
        }

        check_no_crash(rtp_inst, pkt);
    }
}

TEST(RTPFuzzer, boundary_sizes_around_header)
{
    uvgrtp::rtp rtp_inst(RTP_FORMAT_GENERIC, make_ssrc(), false);
    std::mt19937 rng(0xABCDEF01);

    for (size_t len = 0; len <= 30; ++len) {
        for (int trial = 0; trial < 50; ++trial) {
            std::vector<uint8_t> pkt(len);
            for (auto& b : pkt) b = static_cast<uint8_t>(rng());
            if (len >= 1) pkt[0] = (2 << 6) | (rng() & 0x3F);
            check_no_crash(rtp_inst, pkt);
        }
    }
}

TEST(RTPFuzzer, max_length_ext_flood)
{
    uvgrtp::rtp rtp_inst(RTP_FORMAT_GENERIC, make_ssrc(), false);
    std::mt19937 rng(0xCAFED00D);

    for (int trial = 0; trial < 200; ++trial) {
        size_t total = 12 + 4 + (rng() % 200);
        std::vector<uint8_t> pkt(total, 0);

        pkt[0] = (2 << 6) | (1 << 4);
        pkt[8] = 0xAA; pkt[9] = 0xBB; pkt[10] = 0xCC; pkt[11] = 0xDD;

        uint16_t ext_words = static_cast<uint16_t>(rng());
        pkt[12] = (ext_words >> 8) & 0xFF;
        pkt[13] = ext_words & 0xFF;
        pkt[14] = static_cast<uint8_t>(rng());
        pkt[15] = static_cast<uint8_t>(rng());

        for (size_t i = 16; i < total; ++i)
            pkt[i] = static_cast<uint8_t>(rng());

        check_no_crash(rtp_inst, pkt);
    }
}

TEST(RTPFuzzer, padding_with_random_lengths)
{
    uvgrtp::rtp rtp_inst(RTP_FORMAT_GENERIC, make_ssrc(), false);
    std::mt19937 rng(0xBEEFBEEF);

    for (int trial = 0; trial < 300; ++trial) {
        size_t payload_area = 20 + (rng() % 200);
        size_t total = 12 + payload_area;
        std::vector<uint8_t> pkt(total);
        for (auto& b : pkt) b = static_cast<uint8_t>(rng());

        pkt[0] = (2 << 6) | (1 << 5);
        pkt[8] = 0x11; pkt[9] = 0x22; pkt[10] = 0x33; pkt[11] = 0x44;

        pkt[total - 1] = static_cast<uint8_t>(rng());

        check_no_crash(rtp_inst, pkt);
    }
}

TEST(RTPFuzzer, deeply_nested_csrc_ext_padding)
{
    uvgrtp::rtp rtp_inst(RTP_FORMAT_GENERIC, make_ssrc(), false);
    std::mt19937 rng(0x42424242);

    for (int trial = 0; trial < 500; ++trial) {
        uint8_t cc = rng() & 0x0F;
        bool has_ext = (rng() % 2) != 0;
        bool has_pad = (rng() % 2) != 0;

        size_t hdr = 12 + cc * 4;
        size_t ext = has_ext ? (4 + (rng() % 30) * 4) : 0;
        size_t body = rng() % 100;
        size_t pad = has_pad ? (1 + rng() % 10) : 0;
        size_t total = hdr + ext + body + pad;
        if (total > 2000) total = hdr;

        std::vector<uint8_t> pkt(total);
        for (auto& b : pkt) b = static_cast<uint8_t>(rng());

        pkt[0] = (2 << 6) | (has_pad ? (1 << 5) : 0)
               | (has_ext ? (1 << 4) : 0) | (cc & 0x0F);
        pkt[8] = static_cast<uint8_t>(rng());
        pkt[9] = static_cast<uint8_t>(rng());
        pkt[10] = static_cast<uint8_t>(rng());
        pkt[11] = static_cast<uint8_t>(rng());

        if (has_pad && total > 0) {
            pkt[total - 1] = static_cast<uint8_t>(1 + rng() % 20);
        }

        check_no_crash(rtp_inst, pkt);
    }
}
