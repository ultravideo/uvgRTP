// Tests H26x Start Code Lookup code with different offsets

#include <iostream>
#include <cstdint>
#include <cstring>

#include "test_common.hh"

#include "../src/formats/h266.hh"

TEST(FormatTests, h26x_scl) {
    uvgrtp::context ctx;
    uvgrtp::session* local_session = ctx.create_session("127.0.0.1");
    std::shared_ptr<uvgrtp::rtp>    rtp_;
    auto socket_ = std::shared_ptr<uvgrtp::socket>(new uvgrtp::socket(0));
    auto format_26x = uvgrtp::formats::h266(socket_, rtp_, 0);

    for(int offset = 0; offset < 16; offset++) {
      uint8_t data[128];
      memset(data, 128, 128);

      data[offset] = 0;
      data[offset+1] = 0;
      data[offset+2] = 0;
      data[offset+3] = 1;
      data[offset+4] = 0;

      std::cout << "Testing SCL offset " << offset << std::endl;
      uint8_t start_len;
      size_t out = format_26x.find_h26x_start_code(data, 128-offset, 0, start_len);

      EXPECT_EQ(4+offset,(int)out);
    }
}