#include "test_common.hh"

#include "rtp.hh"
#include "uvgrtp/frame.hh"

#include <cstring>
#include <memory>
#include <vector>

static std::vector<uint8_t> make_rtp_packet(
    bool ext_bit, uint8_t cc,
    uint16_t ext_type = 0, uint16_t ext_len_words = 0,
    size_t payload_size = 0, size_t total_override = 0)
{
    size_t hdr_size = 12 + cc * 4;
    size_t ext_size = ext_bit ? (4 + ext_len_words * 4) : 0;
    size_t total = hdr_size + ext_size + payload_size;
    if (total_override > 0)
        total = total_override;

    std::vector<uint8_t> pkt(total, 0);

    pkt[0] = (2 << 6) | (ext_bit ? (1 << 4) : 0) | (cc & 0x0f);
    pkt[1] = 0;
    pkt[2] = 0x00; pkt[3] = 0x01;
    pkt[4] = 0x00; pkt[5] = 0x00; pkt[6] = 0x00; pkt[7] = 0x00;
    pkt[8] = 0x12; pkt[9] = 0x34; pkt[10] = 0x56; pkt[11] = 0x78;

    if (ext_bit && total >= hdr_size + 4) {
        size_t off = hdr_size;
        pkt[off + 0] = (ext_type >> 8) & 0xff;
        pkt[off + 1] = ext_type & 0xff;
        pkt[off + 2] = (ext_len_words >> 8) & 0xff;
        pkt[off + 3] = ext_len_words & 0xff;
    }

    return pkt;
}

static std::shared_ptr<std::atomic<uint32_t>> make_ssrc()
{
    return std::make_shared<std::atomic<uint32_t>>(0x12345678);
}

TEST(RTPParser, ext_header_invalid_len)
{
    uvgrtp::rtp rtp_inst(RTP_FORMAT_GENERIC, make_ssrc(), false);

    // 90 = version=2, ext=1, cc=0
    // 00 00 FF FF = ext type=0, len=65535 words (262140 bytes) but only 4 bytes remain
    std::vector<uint8_t> pkt = {
        0x90, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00,
        0x12, 0x34, 0x56, 0x78,
        0x00, 0x00, 0xFF, 0xFF
    };

    uvgrtp::frame::rtp_frame* out = nullptr;
    rtp_error_t ret = rtp_inst.packet_handler(nullptr, 0, pkt.data(), pkt.size(), &out);

    EXPECT_EQ(ret, RTP_GENERIC_ERROR);
    EXPECT_EQ(out, nullptr);
}

TEST(RTPParser, ext_header_len_exceeds_packet)
{
    uvgrtp::rtp rtp_inst(RTP_FORMAT_GENERIC, make_ssrc(), false);

    std::vector<uint8_t> pkt(20, 0);
    pkt[0] = (2 << 6) | (1 << 4);
    pkt[1] = 0;
    pkt[2] = 0x00; pkt[3] = 0x01;
    pkt[8] = 0x12; pkt[9] = 0x34; pkt[10] = 0x56; pkt[11] = 0x78;
    pkt[12] = 0x12; pkt[13] = 0x34;
    pkt[14] = 0x00; pkt[15] = 0x64;

    uvgrtp::frame::rtp_frame* out = nullptr;
    rtp_error_t ret = rtp_inst.packet_handler(nullptr, 0, pkt.data(), pkt.size(), &out);

    EXPECT_EQ(ret, RTP_GENERIC_ERROR);
    EXPECT_EQ(out, nullptr);
}

TEST(RTPParser, ext_header_len_zero)
{
    uvgrtp::rtp rtp_inst(RTP_FORMAT_GENERIC, make_ssrc(), false);

    auto pkt = make_rtp_packet(true, 0, 0x0001, 0, 4);

    uvgrtp::frame::rtp_frame* out = nullptr;
    rtp_error_t ret = rtp_inst.packet_handler(nullptr, 0, pkt.data(), pkt.size(), &out);

    EXPECT_EQ(ret, RTP_PKT_MODIFIED);
    ASSERT_NE(out, nullptr);
    EXPECT_EQ(out->header.ext, 1);
    EXPECT_EQ(out->ext->len, 0u);
    EXPECT_EQ(out->payload_len, 4u);
    (void)uvgrtp::frame::dealloc_frame(out);
}

TEST(RTPParser, ext_header_exact_fit)
{
    uvgrtp::rtp rtp_inst(RTP_FORMAT_GENERIC, make_ssrc(), false);

    auto pkt = make_rtp_packet(true, 0, 0x0002, 1, 0);

    uvgrtp::frame::rtp_frame* out = nullptr;
    rtp_error_t ret = rtp_inst.packet_handler(nullptr, 0, pkt.data(), pkt.size(), &out);

    EXPECT_EQ(ret, RTP_PKT_MODIFIED);
    ASSERT_NE(out, nullptr);
    EXPECT_EQ(out->header.ext, 1);
    EXPECT_EQ(out->ext->len, 4u);
    EXPECT_EQ(out->payload_len, 0u);
    (void)uvgrtp::frame::dealloc_frame(out);
}

TEST(RTPParser, ext_header_with_csrc)
{
    uvgrtp::rtp rtp_inst(RTP_FORMAT_GENERIC, make_ssrc(), false);

    auto pkt = make_rtp_packet(true, 2, 0x0001, 1, 8);

    uvgrtp::frame::rtp_frame* out = nullptr;
    rtp_error_t ret = rtp_inst.packet_handler(nullptr, 0, pkt.data(), pkt.size(), &out);

    EXPECT_EQ(ret, RTP_PKT_MODIFIED);
    ASSERT_NE(out, nullptr);
    EXPECT_EQ(out->header.cc, 2);
    EXPECT_EQ(out->header.ext, 1);
    EXPECT_EQ(out->ext->len, 4u);
    EXPECT_EQ(out->payload_len, 8u);
    (void)uvgrtp::frame::dealloc_frame(out);
}

TEST(RTPParser, ext_header_with_csrc_oob)
{
    uvgrtp::rtp rtp_inst(RTP_FORMAT_GENERIC, make_ssrc(), false);

    // cc=3, ext=1: 12-byte header + 12-byte CSRC + 4-byte ext header = 28 bytes
    // ext claims 0xFFFF words but 0 bytes remain after ext header
    std::vector<uint8_t> pkt = {
        0x93, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00,
        0x12, 0x34, 0x56, 0x78,
        0x00, 0x00, 0x00, 0x01,  // CSRC 1
        0x00, 0x00, 0x00, 0x02,  // CSRC 2
        0x00, 0x00, 0x00, 0x03,  // CSRC 3
        0xFF, 0xFF, 0xFF, 0xFF   // ext type=0xFFFF, len=0xFFFF words
    };

    uvgrtp::frame::rtp_frame* out = nullptr;
    rtp_error_t ret = rtp_inst.packet_handler(nullptr, 0, pkt.data(), pkt.size(), &out);

    EXPECT_EQ(ret, RTP_GENERIC_ERROR);
    EXPECT_EQ(out, nullptr);
}

TEST(RTPParser, packet_too_small_for_ext_header)
{
    uvgrtp::rtp rtp_inst(RTP_FORMAT_GENERIC, make_ssrc(), false);

    // 91 = version=2, ext=1, cc=1
    // 12-byte header + 4-byte CSRC = 16 bytes, no room for ext header
    std::vector<uint8_t> pkt = {
        0x91, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00,
        0x12, 0x34, 0x56, 0x78,
        0x00, 0x00, 0x00, 0x01
    };

    uvgrtp::frame::rtp_frame* out = nullptr;
    rtp_error_t ret = rtp_inst.packet_handler(nullptr, 0, pkt.data(), pkt.size(), &out);

    EXPECT_EQ(ret, RTP_GENERIC_ERROR);
    EXPECT_EQ(out, nullptr);
}

TEST(RTPParser, no_ext_bit_valid_packet)
{
    uvgrtp::rtp rtp_inst(RTP_FORMAT_GENERIC, make_ssrc(), false);

    auto pkt = make_rtp_packet(false, 0, 0, 0, 100);

    uvgrtp::frame::rtp_frame* out = nullptr;
    rtp_error_t ret = rtp_inst.packet_handler(nullptr, 0, pkt.data(), pkt.size(), &out);

    EXPECT_EQ(ret, RTP_PKT_MODIFIED);
    ASSERT_NE(out, nullptr);
    EXPECT_EQ(out->header.ext, 0);
    EXPECT_EQ(out->payload_len, 100u);
    (void)uvgrtp::frame::dealloc_frame(out);
}

TEST(RTPParser, ext_header_max_valid_length)
{
    uvgrtp::rtp rtp_inst(RTP_FORMAT_GENERIC, make_ssrc(), false);

    auto pkt = make_rtp_packet(true, 0, 0x0001, 50, 10);

    uvgrtp::frame::rtp_frame* out = nullptr;
    rtp_error_t ret = rtp_inst.packet_handler(nullptr, 0, pkt.data(), pkt.size(), &out);

    EXPECT_EQ(ret, RTP_PKT_MODIFIED);
    ASSERT_NE(out, nullptr);
    EXPECT_EQ(out->ext->len, 200u);
    EXPECT_EQ(out->payload_len, 10u);
    (void)uvgrtp::frame::dealloc_frame(out);
}

TEST(RTPParser, ext_header_underflow_payload_len)
{
    uvgrtp::rtp rtp_inst(RTP_FORMAT_GENERIC, make_ssrc(), false);

    // 16-byte packet with ext=1 claiming 0x3FFF words (65532 bytes) of ext data
    // Only 4 bytes available after header -> underflow when subtracting
    std::vector<uint8_t> pkt(16, 0);
    pkt[0] = 0x90; // version=2, ext=1, cc=0
    pkt[8] = 0x12; pkt[9] = 0x34; pkt[10] = 0x56; pkt[11] = 0x78;
    pkt[14] = 0x3F; pkt[15] = 0xFF; // ext len = 0x3FFF words

    uvgrtp::frame::rtp_frame* out = nullptr;
    rtp_error_t ret = rtp_inst.packet_handler(nullptr, 0, pkt.data(), pkt.size(), &out);

    EXPECT_EQ(ret, RTP_GENERIC_ERROR);
    EXPECT_EQ(out, nullptr);
}

TEST(RTPParser, too_small_packet)
{
    uvgrtp::rtp rtp_inst(RTP_FORMAT_GENERIC, make_ssrc(), false);

    std::vector<uint8_t> pkt = {0x80, 0x60, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00};

    uvgrtp::frame::rtp_frame* out = nullptr;
    rtp_error_t ret = rtp_inst.packet_handler(nullptr, 0, pkt.data(), pkt.size(), &out);

    EXPECT_EQ(ret, RTP_PKT_NOT_HANDLED);
    EXPECT_EQ(out, nullptr);
}

TEST(RTPParser, invalid_version)
{
    uvgrtp::rtp rtp_inst(RTP_FORMAT_GENERIC, make_ssrc(), false);

    std::vector<uint8_t> pkt(20, 0);
    pkt[0] = (0 << 6);

    uvgrtp::frame::rtp_frame* out = nullptr;
    rtp_error_t ret = rtp_inst.packet_handler(nullptr, 0, pkt.data(), pkt.size(), &out);

    EXPECT_EQ(ret, RTP_PKT_NOT_HANDLED);
    EXPECT_EQ(out, nullptr);
}

TEST(RTPParser, csrc_oob_check)
{
    uvgrtp::rtp rtp_inst(RTP_FORMAT_GENERIC, make_ssrc(), false);

    std::vector<uint8_t> pkt(20, 0);
    pkt[0] = (2 << 6) | 0x0F;
    pkt[8] = 0x12; pkt[9] = 0x34; pkt[10] = 0x56; pkt[11] = 0x78;

    uvgrtp::frame::rtp_frame* out = nullptr;
    rtp_error_t ret = rtp_inst.packet_handler(nullptr, 0, pkt.data(), pkt.size(), &out);

    EXPECT_EQ(ret, RTP_GENERIC_ERROR);
    EXPECT_EQ(out, nullptr);
}

TEST(RTPParser, padding_invalid)
{
    uvgrtp::rtp rtp_inst(RTP_FORMAT_GENERIC, make_ssrc(), false);

    auto pkt = make_rtp_packet(false, 0, 0, 0, 10);
    pkt[0] |= (1 << 5);
    pkt.back() = 0;

    uvgrtp::frame::rtp_frame* out = nullptr;
    rtp_error_t ret = rtp_inst.packet_handler(nullptr, 0, pkt.data(), pkt.size(), &out);

    EXPECT_EQ(ret, RTP_GENERIC_ERROR);
    EXPECT_EQ(out, nullptr);
}
