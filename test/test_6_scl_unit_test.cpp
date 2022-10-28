// Tests H26x Start Code Lookup code with different offsets

#include <iostream>
#include <cstdint>
#include <cstring>

#include "test_common.hh"

#include "../src/formats/h264.hh"
#include "../src/formats/h266.hh"

const int DATA_SIZE = 128;
const int DATA_VALUE = 128;

TEST(FormatTests, h264_scl_zero) {
    uvgrtp::context ctx;
    uvgrtp::session* local_session = ctx.create_session("127.0.0.1");
    std::shared_ptr<uvgrtp::rtp>    rtp_;
    auto socket_ = std::shared_ptr<uvgrtp::socket>(new uvgrtp::socket(0));
    auto format_26x = uvgrtp::formats::h264(socket_, rtp_, 0);

    for(int offset = 0; offset < 16; offset++) {
      uint8_t data[DATA_SIZE];
      memset(data, DATA_VALUE, DATA_SIZE);

      data[offset] = 0;
      data[offset+1] = 0;
      data[offset+2] = 1;
      data[offset+3] = 0;

      std::cout << "Testing h264 SCL offset " << offset << std::endl;
      uint8_t start_len;
      size_t out = format_26x.find_h26x_start_code(data, DATA_SIZE  - offset, 0, start_len);

      EXPECT_EQ(3 + offset,(int)out);
      EXPECT_EQ(3, start_len);
    }
}

TEST(FormatTests, h264_scl) {
    uvgrtp::context ctx;
    uvgrtp::session* local_session = ctx.create_session("127.0.0.1");
    std::shared_ptr<uvgrtp::rtp>    rtp_;
    auto socket_ = std::shared_ptr<uvgrtp::socket>(new uvgrtp::socket(0));
    auto format_26x = uvgrtp::formats::h264(socket_, rtp_, 0);

    for (int offset = 0; offset < 16; offset++) {
        uint8_t data[DATA_SIZE];
        memset(data, DATA_VALUE, DATA_SIZE);

        data[offset] = 0;
        data[offset + 1] = 0;
        data[offset + 2] = 1;

        std::cout << "Testing h264 SCL offset " << offset << std::endl;
        uint8_t start_len;
        size_t out = format_26x.find_h26x_start_code(data, 128 - offset, 0, start_len);

        EXPECT_EQ(3 + offset, (int)out);
        EXPECT_EQ(3, start_len);
    }
}

TEST(FormatTests, h266_scl_zero) {
    uvgrtp::context ctx;
    uvgrtp::session* local_session = ctx.create_session("127.0.0.1");
    std::shared_ptr<uvgrtp::rtp>    rtp_;
    auto socket_ = std::shared_ptr<uvgrtp::socket>(new uvgrtp::socket(0));
    auto format_26x = uvgrtp::formats::h266(socket_, rtp_, 0);

    for (int offset = 0; offset < 16; offset++) {
        uint8_t data[DATA_SIZE];
        memset(data, DATA_VALUE, DATA_SIZE);

        data[offset] = 0;
        data[offset + 1] = 0;
        data[offset + 2] = 0;
        data[offset + 3] = 1;
        data[offset + 4] = 0;

        std::cout << "Testing h266 SCL offset " << offset << std::endl;
        uint8_t start_len;
        size_t out = format_26x.find_h26x_start_code(data, DATA_SIZE - offset, 0, start_len);

        EXPECT_EQ(4 + offset, (int)out);
        EXPECT_EQ(4, start_len);
    }
}

TEST(FormatTests, h266_scl) {
    uvgrtp::context ctx;
    uvgrtp::session* local_session = ctx.create_session("127.0.0.1");
    std::shared_ptr<uvgrtp::rtp>    rtp_;
    auto socket_ = std::shared_ptr<uvgrtp::socket>(new uvgrtp::socket(0));
    auto format_26x = uvgrtp::formats::h266(socket_, rtp_, 0);

    for (int offset = 0; offset < 16; offset++) {
        uint8_t data[DATA_SIZE];
        memset(data, DATA_VALUE, DATA_SIZE);

        data[offset] = 0;
        data[offset + 1] = 0;
        data[offset + 2] = 0;
        data[offset + 3] = 1;

        std::cout << "Testing h266 SCL offset " << offset << std::endl;
        uint8_t start_len;
        size_t out = format_26x.find_h26x_start_code(data, DATA_SIZE - offset, 0, start_len);

        EXPECT_EQ(4 + offset, (int)out);
        EXPECT_EQ(4, start_len);
    }
}